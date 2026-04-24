import os
import pytest
from conftest import Runner, ResultComplete, ResultError, Macro, compile


def test_malloc_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests):
    mock_source = os.path.join(root_directory, 'mock/malloc.c')
    mock_object = os.path.join(temp_directory, 'malloc.obj')
    compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='malloc', value='tdo_mock_malloc'),
            Macro(name='main', value='tdo_runner_main'),
        ]
    )

    amount = 0
    while True:
        result_or_code, _ = run_tests(f"""
            test::{library}::test_success
            test::{library}::test_success
            test::{library}::test_success
        """, executable=r, args=['--mock-malloc-max', amount])
        if isinstance(result_or_code, list):
            break
        else:
            assert result_or_code.code == 3
        amount += 1


@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_fork_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests):
    mock_source = os.path.join(root_directory, 'mock', 'fork.c')
    mock_object = os.path.join(temp_directory, 'fork.obj')
    compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='fork', value='tdo_mock_fork'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, executable=r, args=['--mock-fork-max', 1])
    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            error='could not create child process',
            step=None,
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            error='could not create child process',
            step=None,
        ),
    ]


@pytest.mark.parametrize('amount', (3, 4, 5))
@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_pipe_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests, amount: int):
    mock_source = os.path.join(root_directory, 'mock', 'pipe.c')
    mock_object = os.path.join(temp_directory, 'pipe.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='pipe', value='tdo_mock_pipe'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-pipe-max', amount])
    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            error='could not setup log redirection',
            step=None,
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            error='could not setup log redirection',
            step=None,
        ),
    ]


@pytest.mark.parametrize('amount', (1, 3, 9, 16))
@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_read_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests, amount: int):
    mock_source = os.path.join(root_directory, 'mock', 'read.c')
    mock_object = os.path.join(temp_directory, 'read.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='read', value='tdo_mock_read'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, executable=r, args=['--mock-read-max', amount])

    found_error = False
    for r in result:
        found_error = found_error or isinstance(r, ResultError)
        if not found_error:
            assert r == ResultComplete(
                file=library,
                name='test_success',
                duration=pytest.approx(0.0, abs=100.0),
                stdout='',
                stderr='',
            )
        else:
            assert r == ResultError(
                file=library,
                name='test_success',
                duration=pytest.approx(0.0, abs=100.0),
                error='could not read output',
                step=None,
            )

    assert len(result) == 3
