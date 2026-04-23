from typing import Generator, List, Optional
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


@pytest.fixture(scope='session')
def library(root_directory: str, temp_directory: str) -> str:
    name = 'library'
    source = f'{name}.c'
    compiled = dynamic_library(name)

    source_path = os.path.join(root_directory, source)
    compiled_path = os.path.join(temp_directory, compiled)
    if not os.path.isfile(source_path):
        raise FileNotFoundError(f'Missing test file: {source}')

    if os.name == 'posix':
        command = f'gcc -shared -fPIC {source_path} -o {compiled_path}'
    elif os.name == 'nt':
        command = f'cl /LD /nologo {source_path} /Fe{compiled_path}'
    else:
        raise NotImplementedError(f'Unknown os: {os.name}')

    with working_directory(temp_directory):
        code = os.system(command)

    if code:
        raise CompileError(f'Could not compile {source}')

    return compiled_path


class Runner:
    compiled_path: Optional[str] = None

    def __init__(self, source: str, temp_directory: str):
        self.source = source
        self.temp_directory = temp_directory

        _, name = os.path.split(source)
        self.name = pathlib.Path(name).with_suffix('')

    def compile(self) -> str:
        if self.compiled_path is not None:
            return self.compiled_path

        compiled_path = executable(os.path.join(self.temp_directory, f'{self.name}_{uuid.uuid4()}'))

        if not os.path.isfile(self.source):
            raise FileNotFoundError(f'Missing test file: {self.source}')

        if os.name == 'posix':
            command = f'gcc -fsanitize=address,undefined -ldl -std=c99 -Werror {self.source} -o {compiled_path}'
        elif os.name == 'nt':
            command = f'cl /nologo {self.source} /Fe{compiled_path}'
        else:
            raise NotImplementedError(f'Unknown os: {os.name}')

        with working_directory(self.temp_directory):
            code = os.system(command)

        if code:
            raise CompileError(f'Could not compile {self.source}')

        self.compiled_path = compiled_path
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
    step: Step


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


@pytest.fixture
def run_tests(runner: Runner):
    def run(tests: str):
        p = subprocess.Popen(
            runner.compile(),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            shell=False,
            text=True,
        )
        out, err = p.communicate(input=tests)

        raw_result = json.loads(out)

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
