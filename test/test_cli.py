from conftest import Error, ErrorCode, RunTests, Mock, MockRunner


def test_default(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, _ = run_tests.args(r)
    assert args == {
        'processes': 1,
        'time_limit': 5.0,
        'single_test': None,
        'test_file': None,
        'output': None,
        'internal_status': None,
        'format': 'json',
        'verbosity': 'none',
        'overwrite': False,
        'stop_on_first_error': False,
    }
    assert code == ErrorCode(code=Error.ok)
