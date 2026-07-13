"""
Custom test runner for Catch2 with PlatformIO
Handles test execution and output parsing for Catch2 v3.7.1
"""

import re
from platformio.test.result import TestCase, TestCaseSource, TestStatus
from platformio.test.runners.base import TestRunnerBase


class Catch2TestCaseParser:
    """Parser for Catch2 test output format"""

    # Catch2 output examples:
    # test/test_button/test_main.cpp:43: Button begins released
    # PASSED
    # test/test_indicator/test_main.cpp:131: Indicator begins off
    # PASSED

    def __init__(self):
        self._test_cases = []
        self._current_test = None
        self._test_output = []

    def parse(self, line):
        """Parse a line from Catch2 output"""
        # Catch2 summary lines like "2 passed (0.01s)"
        if re.match(r'^\d+ passed', line):
            return None

        # Test case line format: "file:line: test name"
        match = re.match(r'^(.+):(\d+):\s+(.+)$', line)
        if match:
            source_file = match.group(1)
            source_line = int(match.group(2))
            test_name = match.group(3)

            if self._current_test:
                self._finalize_current_test()

            self._current_test = {
                'name': test_name,
                'file': source_file,
                'line': source_line,
                'status': None,
                'output': [line]
            }
            return None

        # Status line
        if self._current_test:
            if 'PASSED' in line:
                self._current_test['status'] = TestStatus.PASSED
                self._current_test['output'].append(line)
                return None
            elif 'FAILED' in line:
                self._current_test['status'] = TestStatus.FAILED
                self._current_test['output'].append(line)
                return None
            else:
                self._current_test['output'].append(line)
                return None

        return None

    def _finalize_current_test(self):
        """Finalize the current test case"""
        if self._current_test and self._current_test['status']:
            test_case = TestCase(
                name=self._current_test['name'],
                status=self._current_test['status'],
                source=TestCaseSource(
                    file=self._current_test['file'],
                    line=self._current_test['line']
                ),
                stdout='\n'.join(self._current_test['output']),
            )
            self._test_cases.append(test_case)

    def finalize(self):
        """Finalize parsing and return all test cases"""
        if self._current_test:
            self._finalize_current_test()
        return self._test_cases


class CustomTestRunner(TestRunnerBase):
    """Custom test runner for Catch2"""

    def on_testing_data_output(self, data):
        """Handle output from test execution"""
        parser = Catch2TestCaseParser()

        for line in data.split('\n'):
            line = line.rstrip()
            if not line:
                continue

            # Print output for visibility
            import click
            click.echo(line)

            # Parse test output
            test_case = parser.parse(line)
            if test_case:
                self.test_suite.add_case(test_case)

        # Check if we have accumulated enough output to finalize tests
        for test_case in parser.finalize():
            if test_case not in [tc for tc in self.test_suite.test_cases]:
                self.test_suite.add_case(test_case)
