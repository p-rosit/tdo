import os
import sys
import pytest
from conftest import Error, ResultComplete, ResultError, ErrorCode, RunTests, approx, Mock, MockRunner


def test_malloc_fails(mock_runner: MockRunner, library: str, run_tests: RunTests):
    r = mock_runner(Mock(names=['malloc'], file='malloc.c', override_main=True))

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
            assert result_or_code == ErrorCode(code=Error.memory)
        amount += 1


@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_fork_fails(mock_runner: MockRunner, library: str, run_tests: RunTests):
    r = mock_runner(Mock(names=['fork'], file='fork.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, executable=r, args=['--mock-fork-max', 1])
    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            error='could not create child process',
            step=None,
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            error='could not create child process',
            step=None,
        ),
    ]


@pytest.mark.parametrize('amount', (3, 4, 5))
@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_pipe_fails(mock_runner: MockRunner, library: str, run_tests: RunTests, amount: int):
    r = mock_runner(Mock(names=['pipe'], file='pipe.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-pipe-max', amount])
    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            error='could not setup log redirection',
            step=None,
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            error='could not setup log redirection',
            step=None,
        ),
    ]


@pytest.mark.parametrize('amount', (1, 3, 9, 16))
@pytest.mark.skipif(sys.platform == 'darwin', reason='Hangs on mac for unknown reasons, I do not have hardware to test with')
@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_read_fails(mock_runner: MockRunner, library: str, run_tests: RunTests, amount: int):
    r = mock_runner(Mock(names=['read'], file='read.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, executable=r, args=['--mock-read-max', amount])
    assert isinstance(result, list)

    found_error = False
    for r in result:
        found_error = found_error or isinstance(r, ResultError)
        if not found_error:
            assert r == ResultComplete(
                file=library,
                name='test_success',
                duration=approx(0.0, abs=100.0),
                stdout='',
                stderr='',
            )
        else:
            assert r == ResultError(
                file=library,
                name='test_success',
                duration=approx(0.0, abs=100.0),
                error='could not read output',
                step=None,
            )

    assert len(result) == 3


@pytest.mark.parametrize('amount', (1, 3, 9, 16))
@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_poll_fails(mock_runner: MockRunner, library: str, run_tests: RunTests, amount: int):
    r = mock_runner(Mock(names=['poll'], file='poll.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, executable=r, args=['--mock-poll-max', amount])
    assert isinstance(result, list)

    found_error = False
    for r in result:
        found_error = found_error or isinstance(r, ResultError)
        if not found_error:
            assert r == ResultComplete(
                file=library,
                name='test_success',
                duration=approx(0.0, abs=100.0),
                stdout='',
                stderr='',
            )
        else:
            assert r == ResultError(
                file=library,
                name='test_success',
                duration=approx(0.0, abs=100.0),
                error='could not read output',
                step=None,
            )

    assert len(result) == 3


@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_posix_read_one_byte_at_a_time(mock_runner: MockRunner, library: str, run_tests: RunTests):
    r = mock_runner(Mock(names=['read'], file='posix_one_byte_per_read.c'))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r)
    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
    ]


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_windows_read_one_byte_at_a_time(mock_runner: MockRunner, library: str, run_tests: RunTests):
    r = mock_runner(Mock(names=['ReadFile'], file='windows_one_byte_per_read.c'))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r)
    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
    ]


@pytest.mark.parametrize('amount', (3, 4, 5))
@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_ConnectNamedPipe_fails(mock_runner: MockRunner, library: str, run_tests: RunTests, amount: int):
    r = mock_runner(Mock(names=['ConnectNamedPipe'], file='ConnectNamedPipe.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-connect-max', amount])

    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            error='could not setup log redirection',
            step=None,
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            error='could not setup log redirection',
            step=None,
        ),
    ]


@pytest.mark.parametrize('amount', (2, 3))
@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_CreateFile_fails(mock_runner: MockRunner, library: str, run_tests: RunTests, amount: int):
    r = mock_runner(Mock(names=['CreateFileW', 'CreateFileA'], file='CreateFile.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-create-file-max', amount])

    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            error='could not setup log redirection',
            step=None,
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            error='could not setup log redirection',
            step=None,
        ),
    ]


def test_crash_on_internal_start(mock_runner: MockRunner, library: str, run_tests: RunTests):
    r = mock_runner(Mock(names=[], file='crash_on_internal_start.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
    """, r)

    if os.name == 'posix':
        expected = [
            ResultComplete(
                file=library,
                name='test_success',
                duration=approx(0.0, abs=100.0),
                stdout='',
                stderr='',
            ),
            ResultComplete(
                file=library,
                name='test_success',
                duration=approx(0.0, abs=100.0),
                stdout='',
                stderr='',
            ),
        ]
    elif os.name == 'nt':
        expected = [
            ResultError(
                file=library,
                name='test_success',
                duration=approx(0.0, abs=100.0),
                error='no data in status pipe',
                step=None,
            ),
            ResultError(
                file=library,
                name='test_success',
                duration=approx(0.0, abs=100.0),
                error='no data in status pipe',
                step=None,
            ),
        ]
    else:
        raise NotImplentedError('Unknown platform')

    assert result == expected


@pytest.mark.parametrize('amount', (3, 4, 5))
@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_CreateNamedPipe_fails(mock_runner: MockRunner, library: str, run_tests: RunTests, amount: int):
    r = mock_runner(Mock(names=['CreateNamedPipeW', 'CreateNamedPipeA'], file='CreateNamedPipe.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['-j8', '--mock-create-pipe-max', amount])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_AssignProcessToJobObject_fails(mock_runner: MockRunner, library: str, run_tests: RunTests):
    r = mock_runner(Mock(names=['AssignProcessToJobObject'], file='AssignProcessToJobObject.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-assign-process-max', 1])

    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            error='could not create child process',
            step=None,
        ),
        ResultError(
            file=library,
            name='test_success',
            duration=approx(0.0, abs=100.0),
            error='could not create child process',
            step=None,
        ),
    ]


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_CreateIoCompletionPort_setup_fails(mock_runner: MockRunner, library: str, run_tests: RunTests):
    r = mock_runner(Mock(names=['CreateIoCompletionPort'], file='CreateIoCompletionPort.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-create-port-max', 0])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.parametrize('amount', (4, 5, 6))
@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_CreateIoCompletionPort_fails(mock_runner: MockRunner, library: str, run_tests: RunTests, amount: int):
    r = mock_runner(Mock(names=['CreateIoCompletionPort'], file='CreateIoCompletionPort.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['-j8', '--mock-create-port-max', amount])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_CreateJobObject_fails(mock_runner: MockRunner, library: str, run_tests: RunTests):
    r = mock_runner(Mock(names=['CreateJobObjectW', 'CreateJobObjectA'], file='CreateJobObject.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-create-job-max', 0])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.parametrize('amount', (0, 1))
@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_SetInformationJobObject_fails(mock_runner: MockRunner, library: str, run_tests: RunTests, amount: int):
    r = mock_runner(Mock(names=['SetInformationJobObject'], file='SetInformationJobObject.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-set-job-max', amount])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_GetModuleFileName_fails(mock_runner: MockRunner, library: str, run_tests: RunTests):
    r = mock_runner(Mock(names=['GetModuleFileNameW', 'GetModuleFileNameA'], file='GetModuleFileName.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-get-module-name-max', 0])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_GetQueuedCompletionStatus_fails(mock_runner: MockRunner, library: str, run_tests: RunTests):
    r = mock_runner(Mock(names=['GetQueuedCompletionStatus'], file='GetQueuedCompletionStatus.c', override_main=True))

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-get-queued-max', 2])

    assert result == ErrorCode(code=approx(0, abs=1e20))
