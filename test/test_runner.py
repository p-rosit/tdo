from typing import Callable, Tuple
from conftest import ResultComplete, ResultExit, ResultSignal, StepTest


def test_success(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::test_success')
    assert result == [ResultComplete(
        file=library,
        name='test_success',
        stdout='',
        stderr='',
    )]


def test_success_with_stdout(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::test_success_with_stdout')
    assert result == [ResultComplete(
        file=library,
        name='test_success_with_stdout',
        stdout='Printed\n',
        stderr='',
    )]


def test_success_with_stderr(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::test_success_with_stderr')
    assert result == [ResultComplete(
        file=library,
        name='test_success_with_stderr',
        stdout='',
        stderr='Other thing\n',
    )]


def test_early_exit(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::test_early_exit')
    assert result == [ResultExit(
        file=library,
        name='test_early_exit',
        step=StepTest(file=library, name='test_early_exit'),
        exit=4,
        stdout='',
        stderr='',
    )]


def test_aborts(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::test_aborts')
    assert result == [ResultSignal(
        file=library,
        name='test_aborts',
        step=StepTest(file=library, name='test_aborts'),
        signal=6,
        stdout='',
        stderr='',
    )]
