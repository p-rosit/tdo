from typing import Callable, Tuple
import pytest
from conftest import ResultComplete, ResultError, ResultExit, ResultSignal, StepFixtureAfter, StepFixtureBefore, StepTest


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
        signal=pytest.approx(0, abs=1024),  # the specific signal integer is implementation defined?
        stdout='',
        stderr='',
    )]


def test_fixture_before(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(
        f'test::{library}::test_success_with_other_stdout '
        f'before::{library}::test_success_with_stdout'
    )
    assert result == [ResultComplete(
        file=library,
        name='test_success_with_other_stdout',
        stdout='Printed\nother\n',
        stderr='',
    )]


def test_fixture_after(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(
        f'test::{library}::test_success_with_other_stdout '
        f'after::{library}::test_success_with_stdout'
    )
    assert result == [ResultComplete(
        file=library,
        name='test_success_with_other_stdout',
        stdout='other\nPrinted\n',
        stderr='',
    )]


def test_fixture_multiple(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(
        f'test::{library}::test_success_with_other_stdout '
        f'before::{library}::test_success_with_stdout '
        f'after::{library}::test_success_with_stdout '
        f'before::{library}::test_success_with_stdout '
        f'after::{library}::test_success_with_stdout '
    )
    assert result == [ResultComplete(
        file=library,
        name='test_success_with_other_stdout',
        stdout='Printed\nPrinted\nother\nPrinted\nPrinted\n',
        stderr='',
    )]


def test_all(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_aborts
        test::{library}::test_success_with_stdout
        test::{library}::test_early_exit
        test::{library}::test_success_with_other_stdout
        test::{library}::test_success_with_stderr
    """)
    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            stdout='',
            stderr='',
        ),
        ResultSignal(
            file=library,
            name='test_aborts',
            step=StepTest(file=library, name='test_aborts'),
            signal=pytest.approx(0, abs=1024),
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success_with_stdout',
            stdout='Printed\n',
            stderr='',
        ),
        ResultExit(
            file=library,
            name='test_early_exit',
            step=StepTest(file=library, name='test_early_exit'),
            exit=4,
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success_with_other_stdout',
            stdout='other\n',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success_with_stderr',
            stdout='',
            stderr='Other thing\n',
        ),
    ]


def test_error_load_library_test(run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests('test::library_that_doesn\'t_exist.so::test_name')
    assert result == [ResultError(
        file='library_that_doesn\'t_exist.so',
        name='test_name',
        error='Could not load library: library_that_doesn\'t_exist.so',
        step=StepTest(file='library_that_doesn\'t_exist.so', name='test_name'),
    )]


def test_error_load_library_fixture_before(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f"""
        test::{library}::test_success before::missing_library.so::fixture_name
    """)
    assert result == [ResultError(
        file=library,
        name='test_success',
        error='Could not load library: missing_library.so',
        step=StepFixtureBefore(file='missing_library.so', name='fixture_name'),
    )]


def test_error_load_library_fixture_after(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f"""
        test::{library}::test_success after::missing_library.so::fixture_name
    """)
    assert result == [ResultError(
        file=library,
        name='test_success',
        error='Could not load library: missing_library.so',
        step=StepFixtureAfter(file='missing_library.so', name='fixture_name'),
    )]
