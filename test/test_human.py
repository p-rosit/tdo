import re
from conftest import RunTests, ErrorCode, Error


def test_success(library: str, run_tests: RunTests):
    code, out, err = run_tests.execute(f'test::{library}::test_success', args=['--format', 'human'])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == '[  0%] .\n'
    pattern = (
        r'Reading tests from stdin:\n'
        r'Running 1 tests\n'
        r'Ran 1 tests in \d+\.\d{2} seconds:\n'
        r'    success:   1/1\n'
    )
    assert re.fullmatch(pattern, strip_ansi(err)), err


def test_early_exit(library: str, run_tests: RunTests):
    code, out, err = run_tests.execute(f'test::{library}::test_early_exit', args=['--format', 'human'])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == (
        f'[  0%] UNEXPECTED EXIT (4) {library}::test_early_exit\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
    )
    pattern = (
        r'Reading tests from stdin:\n'
        r'Running 1 tests\n'
        r'Ran 1 tests in \d+\.\d{2} seconds:\n'
        r'    success:   0/1\n'
        r'    failure:   1/1\n'
        r'        exit:      1/1\n'
    )
    assert re.fullmatch(pattern, strip_ansi(err)), err


def test_aborts(library: str, run_tests: RunTests):
    code, out, err = run_tests.execute(f'test::{library}::test_aborts', args=['--format', 'human'])
    assert code == ErrorCode(code=Error.ok)
    pattern = (
        r'\[  0%\] SIGNAL \(-?\d+\) ' + re.escape(library) + r'::test_aborts\n'
        r'Captured stdout ----------------------------------------------------------------\n'
        r'Captured stderr ----------------------------------------------------------------\n'
        r'--------------------------------------------------------------------------------\n'
    )
    assert re.fullmatch(pattern, strip_ansi(out)), out
    pattern = (
        r'Reading tests from stdin:\n'
        r'Running 1 tests\n'
        r'Ran 1 tests in \d+\.\d{2} seconds:\n'
        r'    success:   0/1\n'
        r'    failure:   1/1\n'
        r'        signal:    1/1\n'
    )
    assert re.fullmatch(pattern, strip_ansi(err)), err


def test_error(library: str, run_tests: RunTests):
    code, out, err = run_tests.execute(f'test::{library}::test_name', args=['--format', 'human'])
    assert code == ErrorCode(code=Error.ok)

    lines = strip_ansi(out).splitlines()
    assert f'[  0%] ERROR {library}::test_name' == lines[0]
    assert lines[1].startswith('    Could not load test:')

    pattern = (
        r'Reading tests from stdin:\n'
        r'Running 1 tests\n'
        r'Ran 1 tests in \d+\.\d{2} seconds:\n'
        r'    success:   0/1\n'
        r'    failure:   1/1\n'
        r'        error:     1/1\n'
    )
    assert re.fullmatch(pattern, strip_ansi(err)), err


def test_timeout(library: str, run_tests: RunTests):
    code, out, err = run_tests.execute(f'test::{library}::test_timeout', args=['--timeout', 0.01])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == (
        f'[  0%] TIMEOUT {library}::test_timeout\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Some output\\n\n'
        'Captured stderr ----------------------------------------------------------------\n'
        'Some error\\n\n'
        '--------------------------------------------------------------------------------\n'
    )
    pattern = (
        r'Reading tests from stdin:\n'
        r'Running 1 tests\n'
        r'Ran 1 tests in \d+\.\d{2} seconds:\n'
        r'    success:   0/1\n'
        r'    failure:   1/1\n'
        r'        timeout:   1/1\n'
    )
    assert re.fullmatch(pattern, strip_ansi(err)), err


def test_success_line_break(library: str, run_tests: RunTests):
    code, out, err = run_tests.execute(90 * f"""
        test::{library}::test_success
    """, args=['--format', 'human'])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == (
        '[  0%] ................................................................................\n'
        '[ 88%] ..........\n'
    )
    pattern = (
        r'Reading tests from stdin:\n'
        r'Running 90 tests\n'
        r'Ran 90 tests in \d+\.\d{2} seconds:\n'
        r'    success:  90/90\n'
    )
    assert re.fullmatch(pattern, strip_ansi(err)), err


def test_prints_name(library: str, run_tests: RunTests):
    test_definition = f'test::{library}::test_early_exit after::{library}::test_success_with_stdout before::{library}::test_success'
    code, out, _ = run_tests.execute(test_definition, args=['--format', 'human'])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == (
        f'[  0%] UNEXPECTED EXIT (4) {test_definition}\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
    )


def test_all(library: str, run_tests: RunTests):
    code, out, err = run_tests.execute(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_early_exit
        test::{library}::test_success_with_stdout
        test::{library}::test_early_exit
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success_with_other_stdout
        test::{library}::test_success_with_stderr
    """, args=['--format', 'human'])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == (
        '[  0%] ...\n'
        f'[ 27%] UNEXPECTED EXIT (4) {library}::test_early_exit\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        '[ 36%] .\n'
        f'[ 45%] UNEXPECTED EXIT (4) {library}::test_early_exit\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        '[ 55%] .....\n'
    )
    pattern = (
        r'Reading tests from stdin:\n'
        r'Running 11 tests\n'
        r'Ran 11 tests in \d+\.\d{2} seconds:\n'
        r'    success:   9/11\n'
        r'    failure:   2/11\n'
        r'        exit:      2/2\n'
    )
    assert re.fullmatch(pattern, strip_ansi(err)), err


def test_all_verbose(library: str, run_tests: RunTests):
    code, out, err = run_tests.execute(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_early_exit
        test::{library}::test_success_with_stdout
        test::{library}::test_early_exit
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success_with_other_stdout
        test::{library}::test_success_with_stderr
    """, args=['--format', 'human', '-v'])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == (
        f'[  0%] SUCCESS {library}::test_success\n'
        f'[  9%] SUCCESS {library}::test_success\n'
        f'[ 18%] SUCCESS {library}::test_success\n'
        f'[ 27%] UNEXPECTED EXIT (4) {library}::test_early_exit\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 36%] SUCCESS {library}::test_success_with_stdout\n'
        f'[ 45%] UNEXPECTED EXIT (4) {library}::test_early_exit\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 55%] SUCCESS {library}::test_success\n'
        f'[ 64%] SUCCESS {library}::test_success\n'
        f'[ 73%] SUCCESS {library}::test_success\n'
        f'[ 82%] SUCCESS {library}::test_success_with_other_stdout\n'
        f'[ 91%] SUCCESS {library}::test_success_with_stderr\n'
    )
    pattern = (
        r'Reading tests from stdin:\n'
        r'Running 11 tests\n'
        r'Ran 11 tests in \d+\.\d{2} seconds:\n'
        r'    success:   9/11\n'
        r'    failure:   2/11\n'
        r'        exit:      2/2\n'
    )
    assert re.fullmatch(pattern, strip_ansi(err)), err


def test_all_very_verbose(library: str, run_tests: RunTests):
    code, out, err = run_tests.execute(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_early_exit
        test::{library}::test_success_with_stdout
        test::{library}::test_early_exit
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success_with_other_stdout
        test::{library}::test_success_with_stderr
    """, args=['--format', 'human', '-vv'])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == (
        f'[  0%] SUCCESS {library}::test_success\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[  9%] SUCCESS {library}::test_success\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 18%] SUCCESS {library}::test_success\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 27%] UNEXPECTED EXIT (4) {library}::test_early_exit\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 36%] SUCCESS {library}::test_success_with_stdout\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Printed\\n\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 45%] UNEXPECTED EXIT (4) {library}::test_early_exit\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 55%] SUCCESS {library}::test_success\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 64%] SUCCESS {library}::test_success\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 73%] SUCCESS {library}::test_success\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 82%] SUCCESS {library}::test_success_with_other_stdout\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'other\\n\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 91%] SUCCESS {library}::test_success_with_stderr\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        'Other thing\\n\n'
        '--------------------------------------------------------------------------------\n'
    )
    pattern = (
        r'Reading tests from stdin:\n'
        r'Running 11 tests\n'
        r'Ran 11 tests in \d+\.\d{2} seconds:\n'
        r'    success:   9/11\n'
        r'    failure:   2/11\n'
        r'        exit:      2/2\n'
    )
    assert re.fullmatch(pattern, strip_ansi(err)), err


def strip_ansi(out: str) -> str:
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    return ansi_escape.sub('', out)
