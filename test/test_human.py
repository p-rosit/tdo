import re
from conftest import RunTests, ErrorCode, Error


def test_success(library: str, run_tests: RunTests):
    code, out, _ = run_tests.execute(f'test::{library}::test_success', args=['--format', 'human'])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == '[  0%] .\n'


def test_success_repeat(library: str, run_tests: RunTests):
    code, out, _ = run_tests.execute(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
    """, args=['--format', 'human'])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == '[  0%] ....\n'


def test_success_line_break(library: str, run_tests: RunTests):
    code, out, _ = run_tests.execute(90 * f"""
        test::{library}::test_success
    """, args=['--format', 'human'])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == (
        '[  0%] ................................................................................\n'
        '[ 88%] ..........\n'
    )


def test_timeout(library: str, run_tests: RunTests):
    code, out, _ = run_tests.execute(f'test::{library}::test_timeout', args=['--timeout', 0.01])
    assert code == ErrorCode(code=Error.ok)
    assert strip_ansi(out) == (
        f'[  0%] TIMEOUT {library}::test_timeout\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Some output\\n\n'
        'Captured stderr ----------------------------------------------------------------\n'
        'Some error\\n\n'
        '--------------------------------------------------------------------------------\n'
    )


def test_all(library: str, run_tests: RunTests):
    code, out, _ = run_tests.execute(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test
        test::{library}::test_aborts
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
        f'[ 25%] ERROR {library}::test\n'
        f'    Could not load test: {library}: undefined symbol: test\n'
        f'[ 33%] SIGNAL (6) {library}::test_aborts\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        '[ 42%] .\n'
        f'[ 50%] UNEXPECTED EXIT (4) {library}::test_early_exit\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        '[ 58%] .....\n'
    )


def test_all_verbose(library: str, run_tests: RunTests):
    code, out, _ = run_tests.execute(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test
        test::{library}::test_aborts
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
        f'[  8%] SUCCESS {library}::test_success\n'
        f'[ 17%] SUCCESS {library}::test_success\n'
        f'[ 25%] ERROR {library}::test\n'
        f'    Could not load test: {library}: undefined symbol: test\n'
        f'[ 33%] SIGNAL (6) {library}::test_aborts\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 42%] SUCCESS {library}::test_success_with_stdout\n'
        f'[ 50%] UNEXPECTED EXIT (4) {library}::test_early_exit\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 58%] SUCCESS {library}::test_success\n'
        f'[ 67%] SUCCESS {library}::test_success\n'
        f'[ 75%] SUCCESS {library}::test_success\n'
        f'[ 83%] SUCCESS {library}::test_success_with_other_stdout\n'
        f'[ 92%] SUCCESS {library}::test_success_with_stderr\n'
    )


def test_all_very_verbose(library: str, run_tests: RunTests):
    code, out, _ = run_tests.execute(f"""
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test_success
        test::{library}::test
        test::{library}::test_aborts
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
        f'[  8%] SUCCESS {library}::test_success\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 17%] SUCCESS {library}::test_success\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 25%] ERROR {library}::test\n'
        f'    Could not load test: {library}: undefined symbol: test\n'
        f'[ 33%] SIGNAL (6) {library}::test_aborts\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 42%] SUCCESS {library}::test_success_with_stdout\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Printed\\n\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 50%] UNEXPECTED EXIT (4) {library}::test_early_exit\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 58%] SUCCESS {library}::test_success\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 67%] SUCCESS {library}::test_success\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 75%] SUCCESS {library}::test_success\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 83%] SUCCESS {library}::test_success_with_other_stdout\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'other\\n\n'
        'Captured stderr ----------------------------------------------------------------\n'
        '--------------------------------------------------------------------------------\n'
        f'[ 92%] SUCCESS {library}::test_success_with_stderr\n'
        'Captured stdout ----------------------------------------------------------------\n'
        'Captured stderr ----------------------------------------------------------------\n'
        'Other thing\\n\n'
        '--------------------------------------------------------------------------------\n'
    )


def strip_ansi(out: str) -> str:
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    return ansi_escape.sub('', out)
