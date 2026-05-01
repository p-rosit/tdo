import os
import pytest
from conftest import Error, Runner, ResultComplete, ResultError, Macro, ErrorCode, compile, RunTests


def test_malloc_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests):
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
            assert result_or_code == ErrorCode(code=Error.memory)
        amount += 1


@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_fork_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests):
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
def test_pipe_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests, amount: int):
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
def test_read_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests, amount: int):
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


@pytest.mark.parametrize('amount', (1, 3, 9, 16))
@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_poll_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests, amount: int):
    mock_source = os.path.join(root_directory, 'mock', 'poll.c')
    mock_object = os.path.join(temp_directory, 'poll.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='poll', value='tdo_mock_poll'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, executable=r, args=['--mock-poll-max', amount])

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


@pytest.mark.skipif(os.name != 'posix', reason='Does not run on non-posix system')
def test_posix_read_one_byte_at_a_time(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests):
    mock_source = os.path.join(root_directory, 'mock', 'posix_one_byte_per_read.c')
    mock_object = os.path.join(temp_directory, 'posix_one_byte_per_read.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[Macro(name='read', value='tdo_mock_read')],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r)
    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
    ]


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_windows_read_one_byte_at_a_time(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests):
    mock_source = os.path.join(root_directory, 'mock', 'windows_one_byte_per_read.c')
    mock_object = os.path.join(temp_directory, 'windows_one_byte_per_read.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[Macro(name='ReadFile', value='tdo_mock_ReadFile')],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r)
    assert result == [
        ResultComplete(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
        ResultComplete(
            file=library,
            name='test_success',
            duration=pytest.approx(0.0, abs=100.0),
            stdout='',
            stderr='',
        ),
    ]


@pytest.mark.parametrize('amount', (3, 4, 5))
@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_ConnectNamedPipe_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests, amount: int):
    mock_source = os.path.join(root_directory, 'mock', 'ConnectNamedPipe.c')
    mock_object = os.path.join(temp_directory, 'ConnectNamedPipe.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='ConnectNamedPipe', value='tdo_mock_ConnectNamedPipe'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-connect-max', amount])

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


@pytest.mark.parametrize('amount', (2, 3))
@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_CreateFile_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests, amount: int):
    mock_source = os.path.join(root_directory, 'mock', 'CreateFile.c')
    mock_object = os.path.join(temp_directory, 'CreateFile.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='CreateFileW', value='tdo_mock_CreateFileW'),
            Macro(name='CreateFileA', value='tdo_mock_CreateFileA'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-create-file-max', amount])

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


def test_crash_on_internal_start(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests):
    mock_source = os.path.join(root_directory, 'mock', 'crash_on_internal_start.c')
    mock_object = os.path.join(temp_directory, 'crash_on_internal_start.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[Macro(name='main', value='tdo_runner_main')],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
    """, r)

    if os.name == 'posix':
        expected = [
            ResultComplete(
                file=library,
                name='test_success',
                duration=pytest.approx(0.0, abs=100.0),
                stdout='',
                stderr='',
            ),
            ResultComplete(
                file=library,
                name='test_success',
                duration=pytest.approx(0.0, abs=100.0),
                stdout='',
                stderr='',
            ),
        ]
    elif os.name == 'nt':
        expected = [
            ResultError(
                file=library,
                name='test_success',
                duration=pytest.approx(0.0, abs=100.0),
                error='no data in status pipe',
                step=None,
            ),
            ResultError(
                file=library,
                name='test_success',
                duration=pytest.approx(0.0, abs=100.0),
                error='no data in status pipe',
                step=None,
            ),
        ]
    else:
        raise NotImplentedError('Unknown platform')

    assert result == expected


@pytest.mark.parametrize('amount', (3, 4, 5))
@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_CreateNamedPipe_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests, amount: int):
    mock_source = os.path.join(root_directory, 'mock', 'CreateNamedPipe.c')
    mock_object = os.path.join(temp_directory, 'CreateNamedPipe.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='CreateNamedPipeW', value='tdo_mock_CreateNamedPipeW'),
            Macro(name='CreateNamedPipeA', value='tdo_mock_CreateNamedPipeA'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['-j8', '--mock-create-pipe-max', amount])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_AssignProcessToJobObject_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests):
    mock_source = os.path.join(root_directory, 'mock', 'AssignProcessToJobObject.c')
    mock_object = os.path.join(temp_directory, 'AssignProcessToJobObject.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='AssignProcessToJobObject', value='tdo_mock_AssignProcessToJobObject'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-assign-process-max', 1])

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


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_CreateIoCompletionPort_setup_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests):
    mock_source = os.path.join(root_directory, 'mock', 'CreateIoCompletionPort.c')
    mock_object = os.path.join(temp_directory, 'CreateIoCompletionPort.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='CreateIoCompletionPort', value='tdo_mock_CreateIoCompletionPort'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-create-port-max', 0])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.parametrize('amount', (4, 5, 6))
@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_CreateIoCompletionPort_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests, amount: int):
    mock_source = os.path.join(root_directory, 'mock', 'CreateIoCompletionPort.c')
    mock_object = os.path.join(temp_directory, 'CreateIoCompletionPort.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='CreateIoCompletionPort', value='tdo_mock_CreateIoCompletionPort'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['-j8', '--mock-create-port-max', amount])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_CreateJobObject_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests):
    mock_source = os.path.join(root_directory, 'mock', 'CreateJobObject.c')
    mock_object = os.path.join(temp_directory, 'CreateJobObject.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='CreateJobObjectW', value='tdo_mock_CreateJobObjectW'),
            Macro(name='CreateJobObjectA', value='tdo_mock_CreateJobObjectA'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-create-job-max', 0])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.parametrize('amount', (0, 1))
@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_SetInformationJobObject_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests, amount: int):
    mock_source = os.path.join(root_directory, 'mock', 'SetInformationJobObject.c')
    mock_object = os.path.join(temp_directory, 'SetInformationJobObject.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='SetInformationJobObject', value='tdo_mock_SetInformationJobObject'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-set-job-max', amount])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_GetModuleFileName_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests):
    mock_source = os.path.join(root_directory, 'mock', 'GetModuleFileName.c')
    mock_object = os.path.join(temp_directory, 'GetModuleFileName.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='GetModuleFileNameW', value='tdo_mock_GetModuleFileNameW'),
            Macro(name='GetModuleFileNameA', value='tdo_mock_GetModuleFileNameA'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-get-module-name-max', 0])

    assert result == ErrorCode(code=Error.os)


@pytest.mark.skipif(os.name != 'nt', reason='Does not run on non-windows system')
def test_GetQueuedCompletionStatus_fails(temp_directory: str, root_directory: str, runner: Runner, library: str, run_tests: RunTests):
    mock_source = os.path.join(root_directory, 'mock', 'GetQueuedCompletionStatus.c')
    mock_object = os.path.join(temp_directory, 'GetQueuedCompletionStatus.obj')
    if not os.path.isfile(mock_object):
        compile(temp_directory, [mock_source], mock_object, executable=False)

    r = runner.compile(
        files=[mock_object],
        macros=[
            Macro(name='GetQueuedCompletionStatus', value='tdo_mock_GetQueuedCompletionStatus'),
            Macro(name='main', value='tdo_runner_main'),
        ],
    )

    result, _ = run_tests(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, r, args=['--mock-get-queued-max', 2])

    assert result == ErrorCode(code=pytest.approx(0, abs=1e20))
