import os
import pytest
from conftest import Runner, ResultComplete, ResultError, StepTest, Macro, compile


@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_fork_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests):
    mock_source = os.path.join(root_directory, 'mock_fork.c')
    mock_object = os.path.join(temp_directory, 'mock_fork.obj')
    compile(temp_directory, [mock_source], mock_object, macros=[Macro(name='TDO_FORK_AMOUNT', value=1)], executable=False)
    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, executable=runner.compile(files=[mock_object], macros=[Macro(name='fork', value='tdo_mock_fork')]))
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
