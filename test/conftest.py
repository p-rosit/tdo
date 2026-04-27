from typing import Generator, List, Optional, Any, Dict, Tuple
import dataclasses
import contextlib
import subprocess
import json
import os
import pathlib
import shutil
import uuid
import pytest


def dynamic_library(name: str) -> str:
    if os.name == 'posix':
        extension = 'so'
    elif os.name == 'nt':
        extension = 'dll'
    else:
        raise NotImplementedError(f'Unknown os: {os.name}')
    return f'{name}.{extension}'


def executable(name: str) -> str:
    if os.name == 'posix':
        extension = 'out'
    elif os.name == 'nt':
        extension = 'exe'
    else:
        raise NotImplementedError(f'Unknown os: {os.name}')
    return f'{name}.{extension}'


class CompileError(Exception):
    pass


@dataclasses.dataclass(frozen=True)
class Macro:
    name: str
    value: Optional[Any] = None


def compile(temp_directory: str, files: List[str], output: str, flags: Optional[List[str]] = None, macros: Optional[List[Macro]] = None, dynamic: bool = False, executable: bool = True):
    fs = []
    macros = macros or []
    if os.name == 'posix':
        compiler = 'gcc'
        output_flag = f'-o {output}'
        fs.extend(['-fsanitize=address,undefined', '-std=c99', '-Werror', '-pedantic'])
        if dynamic:
            fs.extend(['-shared', '-fPIC'])
        if not executable:
            fs.append('-c')
        if not dynamic and executable:
            fs.append('-ldl')  # It's probably the main runner...
        for m in macros:
            fs.append(f'-D{m.name}={m.value if m.value is not None else ""}')
    elif os.name == 'nt':
        compiler = 'cl'
        output_flag = f'/Fe{output}'
        fs.append('/nologo')
        if dynamic:
            fs.append('/LD')
        if not executable:
            fs.append('/c')
        for m in macros:
            fs.append(f'/p:{m.name}={m.value if m.value is not None else ""}')
    else:
        raise NotImplementedError(f'Unknown os: {os.name}')

    fs.extend(flags or [])
    command = f'{compiler} {" ".join(files)} {" ".join(fs)} {output_flag}'

    with working_directory(temp_directory):
        code = os.system(command)

    if code:
        raise CompileError('Could not compile')


@pytest.fixture(scope='session')
def library(root_directory: str, temp_directory: str) -> str:
    name = 'library'
    source = f'{name}.c'
    compiled = dynamic_library(name)

    source_path = os.path.join(root_directory, source)
    compiled_path = os.path.join(temp_directory, compiled)
    if not os.path.isfile(source_path):
        raise FileNotFoundError(f'Missing test file: {source}')

    compile(temp_directory, [source_path], output=compiled_path, dynamic=True)
    return compiled_path


class Runner:
    compiled_path: Dict[Tuple[Tuple[str, ...], Tuple[Macro, ...]], str] = {}

    def __init__(self, source: str, temp_directory: str):
        self.source = source
        self.temp_directory = temp_directory

        _, name = os.path.split(source)
        self.name = pathlib.Path(name).with_suffix('')

    def compile(self, files: Optional[List[str]] = None, macros: Optional[List[Macro]] = None) -> str:
        key = (tuple(files or []), tuple(macros or []))
        if (compiled_path := self.compiled_path.get(key, None)) is not None:
            return compiled_path

        compiled_path = executable(os.path.join(self.temp_directory, f'{self.name}_{uuid.uuid4()}'))

        compile(self.temp_directory, [self.source, *(files or [])], compiled_path, macros=macros)
        self.compiled_path[key] = compiled_path
        return compiled_path


@pytest.fixture(scope='session')
def runner(root_directory: str, temp_directory: str) -> Runner:
    source_path = os.path.join(root_directory, '..', 'src', 'runner.c')
    return Runner(source_path, temp_directory)


@dataclasses.dataclass
class Step:
    file: str
    name: str

    @classmethod
    def parse(cls, string: str) -> 'Step':
        step, file, name = string.split('::')
        if step == 'test':
            c = StepTest
        elif step == 'before':
            c = StepFixtureBefore
        elif step == 'after':
            c = StepFixtureAfter
        else:
            raise ValueError(f'Invalid step type: "{step}"')
        return c(file=file, name=name)


@dataclasses.dataclass
class StepTest(Step):
    pass


@dataclasses.dataclass
class StepFixtureBefore(Step):
    pass


@dataclasses.dataclass
class StepFixtureAfter(Step):
    pass


@dataclasses.dataclass
class Result:
    file: str
    name: str
    duration: float


@dataclasses.dataclass
class ResultError(Result):
    error: str
    step: Optional[Step]


@dataclasses.dataclass
class ResultDone(Result):
    stdout: str
    stderr: str

    def __post_init__(self):
        self.stdout = self.stdout.replace('\r\n', '\n')
        self.stderr = self.stderr.replace('\r\n', '\n')


@dataclasses.dataclass
class ResultComplete(ResultDone):
    pass


@dataclasses.dataclass
class ResultExit(ResultDone):
    step: Step
    exit: int


@dataclasses.dataclass
class ResultSignal(ResultDone):
    step: Step
    signal: int


@dataclasses.dataclass
class ResultStop(ResultDone):
    step: Step
    stop: int


@dataclasses.dataclass
class ErrorCode:
    code: int


@pytest.fixture
def run_tests(runner: Runner):
    def run(tests: str, executable: Optional[str] = None, args: Optional[List[str]] = None):
        p = subprocess.Popen(
            [executable or runner.compile(), *[str(a) for a in args or []]],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            shell=False,
            text=True,
        )
        out, err = p.communicate(input=tests)

        try:
            raw_result = json.loads(out)
        except json.JSONDecodeError:
            return ErrorCode(code=p.returncode), err

        result = []
        for r in raw_result:
            c = r.copy()
            if (step := c.get('step', None)) is not None:
                c['step'] = Step.parse(step)

            status = c['status']
            if status == 'error':
                keys = {'file', 'name', 'duration', 'status', 'error', 'step'}
                result_type = ResultError
            elif status == 'complete':
                keys = {'file', 'name', 'duration', 'status', 'stdout', 'stderr'}
                result_type = ResultComplete
            elif status == 'exit':
                keys = {'file', 'name', 'duration', 'status', 'exit', 'stdout', 'stderr', 'step'}
                result_type = ResultExit
            elif status == 'signal':
                keys = {'file', 'name', 'duration', 'status', 'signal', 'stdout', 'stderr', 'step'}
                result_type = ResultSignal
            elif status == 'stop':
                keys = {'file', 'name', 'duration', 'status', 'stop', 'stdout', 'stderr', 'step'}
                result_type = ResultStop
            else:
                raise ValueError(f'Invalid status: "{status}"')

            assert set(c.keys()) == keys
            c = result_type(**{k: c[k] for k in keys if k != 'status'})

            result.append(c)

        return result, err
    return run


@pytest.fixture(scope='session')
def root_directory() -> str:
    return os.path.dirname(__file__)


@pytest.fixture(scope='session')
def temp_directory(root_directory: str) -> Generator[str, None, None]:
    os.mkdir(name := os.path.join(root_directory, f'temp_{uuid.uuid4()}'))
    yield name
    shutil.rmtree(name)


@contextlib.contextmanager
def working_directory(path: str):
    current = os.getcwd()
    os.chdir(path)
    yield
    os.chdir(current)
