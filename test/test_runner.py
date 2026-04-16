from typing import Callable, Tuple
import pytest
from conftest import ResultComplete, ResultError, ResultExit, ResultSignal, StepFixtureAfter, StepFixtureBefore, StepTest


def test_success(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::test_success')
    assert result == [ResultComplete(
        file=library,
        name='test_success',
        duration=pytest.approx(0.0, abs=100.0),
        stdout='',
        stderr='',
    )]


def test_success_with_stdout(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::test_success_with_stdout')
    assert result == [ResultComplete(
        file=library,
        name='test_success_with_stdout',
        duration=pytest.approx(0.0, abs=100.0),
        stdout='Printed\n',
        stderr='',
    )]


def test_success_with_stderr(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::test_success_with_stderr')
    assert result == [ResultComplete(
        file=library,
        name='test_success_with_stderr',
        duration=pytest.approx(0.0, abs=100.0),
        stdout='',
        stderr='Other thing\n',
    )]


def test_early_exit(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::test_early_exit')
    assert result == [ResultExit(
        file=library,
        name='test_early_exit',
        duration=pytest.approx(0.0, abs=100.0),
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
        duration=pytest.approx(0.0, abs=100.0),
        step=StepTest(file=library, name='test_aborts'),
        signal=pytest.approx(0, abs=1e12),  # the specific signal integer is implementation defined?
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
        duration=pytest.approx(0.0, abs=100.0),
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
        duration=pytest.approx(0.0, abs=100.0),
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
        duration=pytest.approx(0.0, abs=100.0),
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
            duration=pytest.approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultSignal(
            file=library,
            name='test_aborts',
            duration=pytest.approx(0.0, abs=100.0),
            step=StepTest(file=library, name='test_aborts'),
            signal=pytest.approx(0, abs=1e12),  # the specific signal integer is implementation defined?
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success_with_stdout',
            duration=pytest.approx(0.0, abs=100.0),
            stdout='Printed\n',
            stderr='',
        ),
        ResultExit(
            file=library,
            name='test_early_exit',
            duration=pytest.approx(0.0, abs=100.0),
            step=StepTest(file=library, name='test_early_exit'),
            exit=4,
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success_with_other_stdout',
            duration=pytest.approx(0.0, abs=100.0),
            stdout='other\n',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success_with_stderr',
            duration=pytest.approx(0.0, abs=100.0),
            stdout='',
            stderr='Other thing\n',
        ),
    ]


def test_error_load_library_test(run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests('test::library_that_doesn\'t_exist.so::test_name')
    assert result == [ResultError(
        file='library_that_doesn\'t_exist.so',
        name='test_name',
        duration=pytest.approx(0.0, abs=100.0),
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
        duration=pytest.approx(0.0, abs=100.0),
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
        duration=pytest.approx(0.0, abs=100.0),
        error='Could not load library: missing_library.so',
        step=StepFixtureAfter(file='missing_library.so', name='fixture_name'),
    )]


def test_error_load_test(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::not_a_test')

    err = result[0].error
    result[0].error = ''

    assert result == [ResultError(
        file=library,
        name='not_a_test',
        duration=pytest.approx(0.0, abs=100.0),
        error='',
        step=StepTest(file=library, name='not_a_test'),
    )]

    assert 'Could not load test: ' in err


def test_error_load_fixture_before(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::test_success before::{library}::not_a_fixture')

    err = result[0].error
    result[0].error = ''

    assert result == [ResultError(
        file=library,
        name='test_success',
        duration=pytest.approx(0.0, abs=100.0),
        error='',
        step=StepFixtureBefore(file=library, name='not_a_fixture'),
    )]

    assert 'Could not load fixture: ' in err


def test_error_load_fixture_after(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, _ = run_tests(f'test::{library}::test_success after::{library}::not_a_fixture')

    err = result[0].error
    result[0].error = ''

    assert result == [ResultError(
        file=library,
        name='test_success',
        duration=pytest.approx(0.0, abs=100.0),
        error='',
        step=StepFixtureAfter(file=library, name='not_a_fixture'),
    )]

    assert 'Could not load fixture: ' in err


def test_parse_missing_library(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, err = run_tests(f"""
        test::::test_success
        test::{library}::test_success
    """)
    assert 'Empty library name' in err
    assert result == [ResultComplete(file=library, name='test_success', duration=pytest.approx(0.0, abs=100.0), stdout='', stderr='')]


def test_parse_missing_name(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, err = run_tests(f"""
        test::library.so::
        test::{library}::test_success
    """)
    assert 'Empty symbol name' in err
    assert result == [ResultComplete(file=library, name='test_success', duration=pytest.approx(0.0, abs=100.0), stdout='', stderr='')]


def test_parse_invalid_prefix(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, err = run_tests(f"""
        thing::library.so::test_name
        test::{library}::test_success
    """)
    assert 'Expected test symbol to start with \'test::\', got \'thing:\'' in err
    assert result == [ResultComplete(file=library, name='test_success', duration=pytest.approx(0.0, abs=100.0), stdout='', stderr='')]


def test_parse_first_not_test(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, err = run_tests(f"""
        after::{library}::test_success test::{library}::test_success
        test::{library}::test_success
    """)
    assert 'Expected test symbol to start with \'test::\', got \'after:\'' in err
    assert result == [ResultComplete(file=library, name='test_success', duration=pytest.approx(0.0, abs=100.0), stdout='', stderr='')]


def test_parse_invalid_fixture(library: str, run_tests: Callable[[str], Tuple[list, str]]):
    result, err = run_tests(f"""
        test::{library}::test_success behind::{library}::test_success
        test::{library}::test_success
    """)
    assert 'Expected fixture symbol to start with \'before::\' or \'after::\', got \'behind::\'' in err
    assert result == [ResultComplete(file=library, name='test_success', duration=pytest.approx(0.0, abs=100.0), stdout='', stderr='')]
