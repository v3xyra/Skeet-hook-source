"""Load the built Windows x86 source DLL into CS:GO, or inspect its startup state."""
import argparse
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import struct
import sys
import time

k = c.WinDLL('kernel32', use_last_error=True)


class ModuleEntry(c.Structure):
    _fields_ = [('size', w.DWORD), ('id', w.DWORD), ('pid', w.DWORD),
                ('global_usage', w.DWORD), ('process_usage', w.DWORD),
                ('base', c.c_void_p), ('image_size', w.DWORD), ('module', c.c_void_p),
                ('name', w.WCHAR * 256), ('path', w.WCHAR * 260)]


def declare(name, result, *arguments):
    function = getattr(k, name)
    function.restype, function.argtypes = result, arguments
    return function


snapshot = declare('CreateToolhelp32Snapshot', w.HANDLE, w.DWORD, w.DWORD)
module_first = declare('Module32FirstW', w.BOOL, w.HANDLE, c.POINTER(ModuleEntry))
module_next = declare('Module32NextW', w.BOOL, w.HANDLE, c.POINTER(ModuleEntry))
close = declare('CloseHandle', w.BOOL, w.HANDLE)
open_process = declare('OpenProcess', w.HANDLE, w.DWORD, w.BOOL, w.DWORD)
query_path = declare('QueryFullProcessImageNameW', w.BOOL, w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD))
read_memory = declare('ReadProcessMemory', w.BOOL, w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t))
write_memory = declare('WriteProcessMemory', w.BOOL, w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t))
allocate = declare('VirtualAllocEx', c.c_void_p, w.HANDLE, c.c_void_p, c.c_size_t, w.DWORD, w.DWORD)
release = declare('VirtualFreeEx', w.BOOL, w.HANDLE, c.c_void_p, c.c_size_t, w.DWORD)
thread = declare('CreateRemoteThread', w.HANDLE, w.HANDLE, c.c_void_p, c.c_size_t, c.c_void_p, c.c_void_p, w.DWORD, c.POINTER(w.DWORD))
wait = declare('WaitForSingleObject', w.DWORD, w.HANDLE, w.DWORD)
exit_code = declare('GetExitCodeThread', w.BOOL, w.HANDLE, c.POINTER(w.DWORD))


def checked(value, operation):
    if not value:
        raise OSError(c.get_last_error(), operation)
    return value


def modules(pid):
    handle = snapshot(0x18, pid)
    if handle == c.c_void_p(-1).value:
        raise OSError(c.get_last_error(), 'Module snapshot')
    try:
        entry = ModuleEntry()
        entry.size = c.sizeof(entry)
        result = {}
        okay = module_first(handle, c.byref(entry))
        while okay:
            result[entry.name.lower()] = (entry.base, entry.image_size, entry.path)
            okay = module_next(handle, c.byref(entry))
        return result
    finally:
        close(handle)


class Process:
    def __init__(self, pid, writable):
        self.pid = pid
        self.handle = checked(open_process(0x410 | (0x2a if writable else 0), False, pid), 'OpenProcess')
        try:
            path = c.create_unicode_buffer(32768)
            size = w.DWORD(len(path))
            checked(query_path(self.handle, 0, path, c.byref(size)), 'QueryFullProcessImageNameW')
            if Path(path.value).name.lower() != 'csgo.exe':
                raise ValueError('Only csgo.exe is supported')
            self.path = path.value
        except Exception:
            close(self.handle)
            raise

    def read(self, address, size):
        buffer = c.create_string_buffer(size)
        count = c.c_size_t()
        checked(read_memory(self.handle, address, buffer, size, c.byref(count)), 'ReadProcessMemory')
        if count.value != size:
            raise RuntimeError('Partial process read')
        return buffer.raw

    def export(self, all_modules, module, name, depth=0):
        if depth > 8:
            raise RuntimeError('Export forwarding cycle')
        base, size, _ = all_modules[module.lower()]
        dos = self.read(base, 64)
        nt = struct.unpack_from('<I', dos, 60)[0]
        header = self.read(base + nt, 0x80)
        if header[:4] != b'PE\0\0' or struct.unpack_from('<H', header, 24)[0] != 0x10b:
            raise RuntimeError('LoadLibrary target module is not PE32')
        rva, extent = struct.unpack_from('<II', header, 0x78)
        directory = struct.unpack('<IIHHIIIIIII', self.read(base + rva, 40))
        function_count, name_count, functions, names, ordinals = directory[6:]
        if max(function_count, name_count) > 65536:
            raise RuntimeError('Invalid export count')
        name_rvas = struct.unpack('<' + 'I' * name_count, self.read(base + names, name_count * 4))
        for index, name_rva in enumerate(name_rvas):
            current = self.read(base + name_rva, min(128, size - name_rva)).split(b'\0', 1)[0]
            if current.decode('ascii') != name:
                continue
            ordinal = struct.unpack('<H', self.read(base + ordinals + index * 2, 2))[0]
            if ordinal >= function_count:
                raise RuntimeError('Invalid export ordinal')
            function = struct.unpack('<I', self.read(base + functions + ordinal * 4, 4))[0]
            if rva <= function < rva + extent:
                forwarded = self.read(base + function, 128).split(b'\0', 1)[0].decode('ascii')
                target, symbol = forwarded.rsplit('.', 1)
                return self.export(all_modules, target.lower() + '.dll', symbol, depth + 1)
            if not 0 < function < size:
                raise RuntimeError('Export exceeds target module')
            return base + function
        raise RuntimeError(f'Missing {module}!{name}')

    def load(self, dll):
        all_modules = modules(self.pid)
        if dll.name.lower() in all_modules:
            raise RuntimeError('The source DLL is already loaded; inspect it instead of acquiring another reference')
        for required in ('client.dll', 'engine.dll', 'serverbrowser.dll', 'gameoverlayrenderer.dll'):
            if required not in all_modules:
                raise RuntimeError(f'Game startup is not ready: {required} is absent')
        entry = self.export(all_modules, 'kernel32.dll', 'LoadLibraryW')
        path = (str(dll) + '\0').encode('utf-16le')
        remote = checked(allocate(self.handle, None, len(path), 0x3000, 0x04), 'VirtualAllocEx path')
        worker = None
        completed = False
        try:
            count = c.c_size_t()
            checked(write_memory(self.handle, remote, path, len(path), c.byref(count)), 'WriteProcessMemory DLL path')
            if count.value != len(path):
                raise RuntimeError('Partial DLL path write')
            worker = checked(thread(self.handle, None, 0, entry, remote, 0, None), 'CreateRemoteThread LoadLibraryW')
            result = wait(worker, 15000)
            if result != 0:
                raise RuntimeError(f'LoadLibrary thread did not finish (wait={result:#x}); its path allocation is retained')
            completed = True
            code = w.DWORD()
            checked(exit_code(worker, c.byref(code)), 'GetExitCodeThread')
            if not code.value:
                raise RuntimeError('Windows LoadLibraryW rejected the source DLL')
        finally:
            if completed or not worker:
                release(self.handle, remote, 0, 0x8000)
            if worker:
                close(worker)


def diagnostic_rvas(dll):
    try:
        import capstone
        import pefile
    except ImportError as error:
        requirements = Path(__file__).with_name('requirements-loader.txt')
        raise RuntimeError(
            f'Missing loader dependency. Run: python -m pip install -r "{requirements}"'
        ) from error
    try:
        pe = pefile.PE(str(dll))
    except pefile.PEFormatError as error:
        raise ValueError('The requested DLL is not a valid PE image') from error
    try:
        return image_diagnostics(pe, capstone)
    finally:
        pe.close()


def image_diagnostics(pe, capstone):
    if (pe.FILE_HEADER.Machine != 0x14c or pe.OPTIONAL_HEADER.Magic != 0x10b
            or not pe.FILE_HEADER.Characteristics & 0x2000):
        raise ValueError('The source client must be a Windows x86 DLL')
    symbols = getattr(getattr(pe, 'DIRECTORY_ENTRY_EXPORT', None), 'symbols', ())
    exports = {entry.name: entry.address for entry in symbols}
    required = {b'SkeetGetStatus', b'SkeetCopyStartupError'}
    if not required.issubset(exports):
        raise ValueError('This DLL does not export the source client diagnostics')
    getter = pe.get_data(exports[b'SkeetGetStatus'], 6)
    if len(getter) != 6 or getter[0] != 0xa1 or getter[5] != 0xc3:
        raise RuntimeError('Unrecognized source status getter; no guessed process offsets are used')
    state = struct.unpack_from('<I', getter, 1)[0] - pe.OPTIONAL_HEADER.ImageBase
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    decoder.detail = True
    candidates = set()
    for instruction in decoder.disasm(pe.get_data(exports[b'SkeetCopyStartupError'], 128), 0):
        if instruction.mnemonic == 'ret':
            break
        if instruction.mnemonic != 'mov' or len(instruction.operands) != 2:
            continue
        operand = instruction.operands[1]
        if operand.type != capstone.CS_OP_IMM:
            continue
        rva = operand.imm - pe.OPTIONAL_HEADER.ImageBase
        section = pe.get_section_by_rva(rva)
        if section is not None and section.Characteristics & 0x80000000:
            candidates.add(rva)
    if len(candidates) != 1:
        raise RuntimeError('Source error buffer is not uniquely identified by its actual export')
    error = candidates.pop()
    for rva, size in ((state, 4), (error, 4096)):
        section = pe.get_section_by_rva(rva)
        if (section is None or not section.Characteristics & 0x80000000
                or rva + size > section.VirtualAddress + max(section.Misc_VirtualSize, section.SizeOfRawData)):
            raise RuntimeError('Source diagnostic storage exceeds its writable image section')
    return state, error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, help='Target csgo.exe process ID (required unless --check)')
    parser.add_argument('--dll', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'build/native-core-x86/bin/skeet_client.dll',
                        help='Built source DLL; defaults to build/native-core-x86/bin/skeet_client.dll')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--inspect', action='store_true', help='Read startup status without loading the DLL')
    mode.add_argument('--check', action='store_true', help='Validate the DLL without opening any game process')
    parser.add_argument('--reuse', action='store_true',
                        help='Inspect an already loaded source DLL instead of loading it twice')
    args = parser.parse_args()
    if not args.check and (args.pid is None or args.pid <= 0):
        parser.error('--pid must identify a running csgo.exe unless --check is used')
    dll = args.dll.resolve(strict=True)
    if not dll.is_file() or dll.name.lower() != 'skeet_client.dll':
        raise ValueError('Select the built skeet_client.dll')
    state_rva, error_rva = diagnostic_rvas(dll)
    if args.check:
        print(f'{dll.name}: OK', flush=True)
        return 0
    loaded = modules(args.pid).get(dll.name.lower())
    if loaded:
        if Path(loaded[2]).resolve() != dll:
            raise RuntimeError(f'A source DLL is already loaded from {loaded[2]}. Restart CS:GO to use {dll}.')
        if args.reuse:
            args.inspect = True
        elif not args.inspect:
            raise RuntimeError('The source DLL is already loaded; use --inspect or --reuse')
    elif args.inspect:
        raise RuntimeError('The source DLL is not loaded in this process')
    process = Process(args.pid, not args.inspect)
    try:
        if not args.inspect:
            process.load(dll)
        deadline = time.monotonic() + 30
        previous = None
        while True:
            loaded = modules(args.pid).get(dll.name.lower())
            if loaded is None:
                raise RuntimeError('The source DLL is no longer loaded in this process')
            base, _, loaded_path = loaded
            if Path(loaded_path).resolve() != dll:
                raise RuntimeError('The loaded module path differs from the requested source DLL')
            state = struct.unpack('<I', process.read(base + state_rva, 4))[0]
            if state != previous:
                if state == 1:
                    print('Initializing...', flush=True)
                previous = state
            if state in (3, 4):
                reason = process.read(base + error_rva, 4096).split(b'\0', 1)[0].decode('utf-8', 'replace')
                print(reason or ('Startup failed.' if state == 3 else 'Cleanup failed.'), file=sys.stderr, flush=True)
                return 2
            if state != 1 or time.monotonic() >= deadline:
                if state == 2:
                    print('Already loaded.' if args.inspect else 'Loaded.', flush=True)
                    return 0
                print('Initialization timed out.' if state == 1 else f'Not ready (status {state}).', file=sys.stderr, flush=True)
                return 3
            time.sleep(0.25)
    finally:
        close(process.handle)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, struct.error) as error:
        print(f'Loader error: {error}', file=sys.stderr, flush=True)
        raise SystemExit(1)
