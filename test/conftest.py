from typing import Generator
import dataclasses
import contextlib
import subprocess
import json
import os
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


@pytest.fixture(scope='session')
def runner(root_directory: str, temp_directory: str) -> str:
    name = 'runner'
    source = f'{name}.c'
    compiled = executable(name)

    source_path = os.path.join(root_directory, '..', 'src', source)
    compiled_path = os.path.join(temp_directory, compiled)

    if not os.path.isfile(source_path):
        raise FileNotFoundError(f'Missing test file: {source}')

    if os.name == 'posix':
        command = f'gcc -fsanitize=address,undefined -ldl -std=c99 -Werror {source_path} -o {compiled_path}'
    elif os.name == 'nt':
        command = f'cl /nologo {source_path} /Fe{compiled_path}'
    else:
        raise NotImplementedError(f'Unknown os: {os.name}')

    with working_directory(temp_directory):
        code = os.system(command)

    if code:
        raise CompileError(f'Could not compile {source}')

    return compiled_path


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
def run_tests(runner: str):
    def run(tests: str):
        p = subprocess.Popen(
            runner,
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
