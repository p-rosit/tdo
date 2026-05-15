from typing import Callable, Generator, List, Optional, Any, Tuple, Union, TYPE_CHECKING
import re
import enum
import functools
import dataclasses
import contextlib
import subprocess
import json
import os
import pathlib
import shutil
import uuid
import pytest


def pytest_addoption(parser):
    parser.addoption("--compiler", action="store")
    parser.addoption("--optimization", action="store", default="debug")


def pytest_generate_tests(metafunc):
    if metafunc.config.getoption("--compiler") is None:
        raise ValueError('Missing compiler, specify with `--compiler gcc,cl,clang,tcc`')

    metafunc.parametrize(
        "compiler",
        metafunc.config.getoption("--compiler").split(","),
    )
    metafunc.parametrize(
        "optimization",
        [Optimization[level] for level in metafunc.config.getoption("--optimization").split(",")],
    )


if TYPE_CHECKING:
    from typing import TypeVar
    T = TypeVar('T')

    def approx(expected: T, rel=None, abs=None, nan_ok=False) -> T:
        pass
else:
    from pytest import approx


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

    def as_flag(self, compiler: str) -> str:
        if compiler in ['gcc', 'clang']:
            return f'-D{self.name}={self.value or ""}'
        if compiler in ['cl']:
            return f'/D{self.name}={self.value or ""}'
        raise NotImplementedError(f'Unkown compiler: "{self}"')


class Optimization(enum.Enum):
    debug = enum.auto()
    minor = enum.auto()
    major = enum.auto()

    def as_flag(self, compiler: str) -> str:
        if compiler in ['gcc', 'clang']:
            if self == Optimization.debug:
                return '-O0'
            if self == Optimization.minor:
                return '-O1'
            if self == Optimization.major:
                return '-O2'
            raise NotImplementedError(f'Unknown optimization level: {self}')
        if compiler in ['cl']:
            if self == Optimization.debug:
                return '/Od'
            if self == Optimization.minor:
                return '/O1'
            if self == Optimization.major:
                return '/O2'
            raise NotImplementedError(f'Unknown optimization level: {self}')
        raise NotImplementedError(f'Unkown compiler: "{self}"')


class Executable(enum.Enum):
    executable = enum.auto()
    dynamic = enum.auto()
    object = enum.auto()

    def as_flag(self, compiler: str) -> str:
        if compiler in ['gcc', 'clang']:
            if self == Executable.executable:
                return ''
            if self == Executable.dynamic:
                # `-fPIC` has no meaning on windows since all executables are
                # position independent
                return '-shared' + ('' if os.name == 'nt' else ' -fPIC')
            if self == Executable.object:
                return '-c'
            raise NotImplementedError(f'Unknown executable type: {self}')
        if compiler in ['cl']:
            if self == Executable.executable:
                return ''
            if self == Executable.dynamic:
                return '/LD'
            if self == Executable.object:
                return '/c'
            raise NotImplementedError(f'Unknown executable type: {self}')
        raise NotImplementedError(f'Unkown compiler: "{self}"')

    def output_flag(self, compiler: str, name: str) -> str:
        if compiler in ['gcc', 'clang']:
            return f'-o {name}'
        if compiler in ['cl']:
            if self == Executable.executable:
                return f'/Fe{name}'
            if self == Executable.dynamic:
                return f'/Fe{name}'
            if self == Executable.object:
                return f'/Fo{name}'
            raise NotImplementedError(f'Unknown executable type: {self}')
        raise NotImplementedError(f'Unkown compiler: "{self}"')


@dataclasses.dataclass
class CompilerCommand:
    compiler: str
    output: str
    optimization: Optimization
    files: List[str]
    flags: List[str] = dataclasses.field(default_factory=list)
    macros: List[Macro] = dataclasses.field(default_factory=list)
    result: Executable = Executable.executable

    def __post_init__(self):
        fs = []

        if self.compiler in ['gcc', 'clang']:
            fs.extend(['-fsanitize=address,undefined', '-std=c99', '-pedantic'])
            if os.name != 'nt':
                fs.append('-Werror')

            if self.result == Executable.executable and os.name != 'nt':
                fs.append('-ldl')  # It's probably the main executable...
        elif self.compiler == 'cl':
            fs.append('/nologo')
        else:
            raise NotImplementedError(f'Unknown compiler: "{self.compiler}"')

        fs.append(self.result.as_flag(self.compiler))
        fs.append(self.optimization.as_flag(self.compiler))
        for m in sorted(self.macros, key=lambda x: x.name):
            fs.append(m.as_flag(self.compiler))

        fs.extend(self.flags)
        self.command = f'{self.compiler} {" ".join(sorted(self.files))} {" ".join(fs)} {self.result.output_flag(self.compiler, self.output)}'

    def __hash__(self):
        return hash(self.command)


@functools.cache
def compile(directory: str, cc: CompilerCommand):
    if os.name == 'nt' and cc.compiler not in ['cl', 'clang']:
        pytest.skip(f'Compiler "{compiler}" not available on windows')
    elif os.name == 'posix' and cc.compiler not in ['gcc', 'clang']:
        pytest.skip(f'Compiler "{cc.compiler}" not available on posix')

    with working_directory(directory):
        code = os.system(cc.command)

    if code:
        raise CompileError('Could not compile')


@pytest.fixture
def compiler(request) -> str:
    return request.param


@pytest.fixture
def optimization(request) -> Optimization:
    return request.param


@pytest.fixture
def library(compiler: str, root_directory: str, temp_directory: str) -> str:
    name = 'library'
    source = f'{name}.c'
    compiled = dynamic_library(f'{compiler}_{name}')

    source_path = os.path.join(root_directory, source)
    compiled_path = os.path.join(temp_directory, compiled)

    compile(temp_directory, CompilerCommand(
        compiler=compiler,
        output=compiled_path,
        optimization=Optimization.debug,
        files=[source_path],
        result=Executable.dynamic,
    ))
    return compiled_path


class Runner:
    def __init__(self, compiler: str, optimization: Optimization, temp_directory: str, root_directory: str):
        self.name = 'tdo'
        self.source = os.path.join(root_directory, '..', 'src', 'main.c')
        self.compiler = compiler
        self.optimization = optimization
        self.temp_directory = temp_directory

    def __call__(self) -> str:
        name = f'{self.compiler}_{self.optimization.name}_{self.name}'
        compiled_path = executable(os.path.join(self.temp_directory, name))
        compile(self.temp_directory, CompilerCommand(
            compiler=self.compiler,
            output=compiled_path,
            optimization=self.optimization,
            files=[self.source],
            result=Executable.executable,
        ))
        return compiled_path


@pytest.fixture
def runner(compiler: str, optimization: Optimization, root_directory: str, temp_directory: str) -> Runner:
    return Runner(compiler, optimization, temp_directory, root_directory)


@dataclasses.dataclass
class Mock:
    names: List[str]
    file: str
    defined_in: Optional[str] = None
    override_main: bool = False


class MockRunner:
    def __init__(self, compiler: str, optimization: Optimization, source_paths: List[str], temp_directory: str, root_directory: str):
        self.name = 'tdo'
        self.compiler = compiler
        self.optimization = optimization
        self.temp_directory = temp_directory
        self.root_directory = root_directory
        self.source_paths = source_paths

    def __call__(self, func: Mock) -> str:
        mock_source = os.path.join(self.root_directory, f'mock/{func.file}')
        mock_object = os.path.join(self.temp_directory, f'{self.compiler}_{self.optimization.name}_{pathlib.Path(func.file).with_suffix(".obj")}')

        compile(self.temp_directory, CompilerCommand(
            compiler=self.compiler,
            output=mock_object,
            optimization=self.optimization,
            files=[mock_source],
            result=Executable.object,
        ))

        macros = [Macro(name=name, value=f'tdo_mock_{name}') for name in func.names]
        if func.override_main:
            macros.append(Macro(name='main', value='tdo_runner_main'))

        names = set(os.path.split(path)[1] for path in self.source_paths)
        assert func.defined_in is None or func.defined_in in names, f'Mocking function expected to be defined in "{func.file}" but no such source file exists'

        object_paths = []
        for path in self.source_paths:
            _, name = os.path.split(path)

            object_path = os.path.join(self.temp_directory, f'{self.compiler}_{self.optimization.name}_{pathlib.Path(name).with_suffix("")}.obj')
            object_paths.append(object_path)

            if func.defined_in != name:
                m = macros
            else:
                m = []

            compile(self.temp_directory, CompilerCommand(
                compiler=self.compiler,
                output=object_path,
                optimization=self.optimization,
                files=[path],
                result=Executable.object,
                macros=[Macro(name='TDO_BUILD_TEST'), *m],
            ))

        compiled_path = executable(os.path.join(self.temp_directory, f'{self.compiler}_{self.optimization.name}_{self.name}_mock_{"_".join(func.names)}'))

        compile(self.temp_directory, CompilerCommand(
            compiler=self.compiler,
            output=compiled_path,
            optimization=self.optimization,
            files=[*object_paths, mock_object],
            result=Executable.executable,
            macros=macros or [],
        ))
        return compiled_path


@pytest.fixture
def mock_runner(compiler: str, optimization: Optimization, root_directory: str, temp_directory: str):
    sources = [
        'arena.c',
        'arguments.c',
        'main.c',
        'platform.c',
        'run.c',
        'str.c',
        'test.c',
    ]
    source_paths = [
        os.path.join(root_directory, '..', 'src', name)
        for name in sources
    ]
    return MockRunner(compiler, optimization, source_paths, temp_directory, root_directory)


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
class ResultTimeout(ResultDone):
    step: Step


class Error(enum.Enum):
    ok = 0
    unknown = -1
    arg_first = 1
    arg_parse = 2
    memory = 3
    file = 4
    input = 5
    eof = 6
    pipe = 7
    newline = 8
    negative = 9
    number = 10
    prefix = 11
    would_block = 12
    os = 13


@dataclasses.dataclass
class ErrorCode:
    code: Union[Error, int]

    def __post_init__(self):
        if isinstance(self.code, Error):
            self.code = self.code.value


class RunTests:
    def __init__(self, function: Callable):
        self.function = function

    def __call__(self, tests: str, executable: Optional[str] = None, args: Optional[List[Any]] = None) -> Tuple[Union[List[Result], ErrorCode], str]:
        return self.function(tests, executable=executable, args=args)


def strip_asan_noise(text: str) -> str:
    """
    Removes ASan / interception_win warnings from stderr.
    Example noise: ==2544==interception_win: unhandled instruction...
    """
    if not text:
        return ""

    # Matches lines starting with ==digits== followed by interception_win or asan
    # and removes the entire line including the trailing newline.
    pattern = r"==\d+==(?:interception_win|asan|AddressSanitizer):.*?\n"

    return re.sub(pattern, "", text, flags=re.IGNORECASE)


@pytest.fixture
def run_tests(runner: Runner):
    def run(tests: str, executable: Optional[str] = None, args: Optional[List[Any]] = None):
        p = subprocess.Popen(
            [executable or runner(), '--format', 'json', *[str(a) for a in args or []]],
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
            assert p.returncode != 0, f'Successful run returned malformed json: "{out}"'
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
            elif status == 'timeout':
                keys = {'file', 'name', 'duration', 'status', 'stdout', 'stderr', 'step'}
                result_type = ResultTimeout
            else:
                raise ValueError(f'Invalid status: "{status}"')

            if 'stderr' in c:
                c['stderr'] = strip_asan_noise(c['stderr'])

            assert set(c.keys()) == keys
            c = result_type(**{k: c[k] for k in keys if k != 'status'})

            result.append(c)

        return result, err
    return RunTests(run)


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
