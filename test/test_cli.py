from conftest import Error, ErrorCode, RunTests, Mock, MockRunner, Runner


def test_default(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r)
    assert args == {
        'processes': 1,
        'time_limit': 5.0,
        'single_test': None,
        'test_file': None,
        'output': None,
        'format': 'json',
        'verbosity': 'none',
        'overwrite': False,
        'stop_on_first_error': False,
        'internal_status': None,
    }
    assert err == ''
    assert code == ErrorCode(code=Error.ok)


def test_help(runner: Runner, run_tests: RunTests):
    code, _, err = run_tests.args(runner(), args=['-h'])
    assert err.startswith('Usage: tdo')
    assert code == ErrorCode(code=Error.ok)

    code, _, err = run_tests.args(runner(), args=['--help'])
    assert err.startswith('Usage: tdo')
    assert code == ErrorCode(code=Error.ok)


def test_processes(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=['-j3'])
    assert args['processes'] == 3
    assert err == ''
    assert code == ErrorCode(code=Error.ok)

    code, args, err = run_tests.args(r, args=['-j', '4'])
    assert args['processes'] == 4
    assert err == ''
    assert code == ErrorCode(code=Error.ok)


def test_processes_invalid_number(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, _, err = run_tests.args(r, args=['-jdata'])
    assert err == 'Could not parse amount of threads: \'data\'\n'
    assert code == ErrorCode(code=Error.arg_parse)


def test_processes_half_number(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, _, err = run_tests.args(r, args=['-j5d'])
    assert err == 'Could not parse amount of threads: \'5d\'\n'
    assert code == ErrorCode(code=Error.arg_parse)


def test_processes_no_number(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, _, err = run_tests.args(r, args=['-j'])
    assert err == 'Missing argument to \'-j\'\n'
    assert code == ErrorCode(code=Error.arg_parse)


def test_time_limit(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=['--timeout', 0.00001])
    assert args['time_limit'] == 0.00001
    assert err == ''
    assert code == ErrorCode(code=Error.ok)


def test_negative_time(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=['--timeout', -0.4])
    assert args['time_limit'] == 5.0
    assert 'Timeout must be strictly positive, got -0.400000\n' == err
    assert code == ErrorCode(code=Error.arg_parse)


def test_no_time(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, _, err = run_tests.args(r, args=['--timeout'])
    assert 'Missing argument to \'--timeout\'\n' == err
    assert code == ErrorCode(code=Error.arg_parse)


def test_single_test(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=['-t', 'test::library::name before::library::other'])
    assert args['single_test'] == 'test::library::name before::library::other'
    assert '' == err
    assert code == ErrorCode(code=Error.ok)


def test_test_file(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=['./path/to/args.txt'])
    assert args['test_file'] == './path/to/args.txt'
    assert '' == err
    assert code == ErrorCode(code=Error.ok)


def test_both_test_file_and_single_test(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=['-t', 'test::library::name before::library::other', 'args.txt'])
    assert args['test_file'] == 'args.txt'
    assert args['single_test'] == 'test::library::name before::library::other'
    assert 'Only specify an input file or a single test to run, not both\n' == err
    assert code == ErrorCode(code=Error.arg_parse)


def test_output(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=['-o', 'some_file.txt'])
    assert args['output'] == 'some_file.txt'
    assert '' == err
    assert code == ErrorCode(code=Error.ok)


def test_format(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=['--format', 'human'])
    assert args['format'] == 'human'
    assert '' == err
    assert code == ErrorCode(code=Error.ok)

    code, args, err = run_tests.args(r, args=['--format', 'json'])
    assert args['format'] == 'json'
    assert '' == err
    assert code == ErrorCode(code=Error.ok)


def test_format_invalid(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, _, err = run_tests.args(r, args=['--format', 'something_else'])
    assert 'Unknown format argument \'something_else\', see \'-h\' for options\n' == err
    assert code == ErrorCode(code=Error.arg_parse)


def test_verbosity(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=[])
    assert args['verbosity'] == 'none'
    assert '' == err
    assert code == ErrorCode(code=Error.ok)

    code, args, err = run_tests.args(r, args=['-v'])
    assert args['verbosity'] == 'minor'
    assert '' == err
    assert code == ErrorCode(code=Error.ok)

    code, args, err = run_tests.args(r, args=['-vv'])
    assert args['verbosity'] == 'major'
    assert '' == err
    assert code == ErrorCode(code=Error.ok)


def test_verbosity_invalid(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, _, err = run_tests.args(r, args=['-vvv'])
    assert 'Unrecognized argument: \'-vvv\'\n' == err
    assert code == ErrorCode(code=Error.arg_parse)


def test_overwrite(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=[])
    assert not args['overwrite']
    assert '' == err
    assert code == ErrorCode(code=Error.ok)

    code, args, err = run_tests.args(r, args=['-f'])
    assert args['overwrite']
    assert '' == err
    assert code == ErrorCode(code=Error.ok)


def test_stop_on_first_error(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=[])
    assert not args['stop_on_first_error']
    assert '' == err
    assert code == ErrorCode(code=Error.ok)

    code, args, err = run_tests.args(r, args=['-x'])
    assert args['stop_on_first_error']
    assert '' == err
    assert code == ErrorCode(code=Error.ok)


def test_internal_status(mock_runner: MockRunner, run_tests: RunTests):
    r = mock_runner(Mock(names=['tdo_arguments_parse'], file='output_cli.c', defined_in='arguments.c'))

    code, args, err = run_tests.args(r, args=['--internal-status', 'path/to/pipe'])
    assert args['internal_status'] == 'path/to/pipe'
    assert '' == err
    assert code == ErrorCode(code=Error.ok)
