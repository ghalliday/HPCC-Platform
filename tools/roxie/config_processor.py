#!/usr/bin/env python3
"""
Configurat  ion File Processor

Processes configuration files with multiple sections and applies environment
variable replacements to XML files.
"""

import os
import sys
import argparse
import re
import subprocess
from pathlib import Path
from typing import Dict, List, Tuple


class ConfigSection:
    """Represents a section in the configuration file."""
    
    def __init__(self, name: str):
        self.name = name.lower()
        self.lines = []
    
    def add_line(self, line: str):
        """Add a line to this section."""
        self.lines.append(line)


class ConfigFile:
    """Parses and represents a configuration file."""
    
    def __init__(self, filepath: str):
        self.filepath = filepath
        self.basename = Path(filepath).stem
        self.sections = {}
        self._parse()
    
    def _parse(self):
        """Parse the configuration file into sections."""
        current_section = None
        
        with open(self.filepath, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.rstrip('\n\r')
                
                # Skip comment lines starting with #
                if line.strip().startswith('#'):
                    continue
                
                # Check for section header
                section_match = re.match(r'^\[([^\]]+)\]$', line.strip())
                if section_match:
                    section_name = section_match.group(1).lower()
                    current_section = ConfigSection(section_name)
                    self.sections[section_name] = current_section
                elif current_section is not None and line.strip():
                    # Add non-empty lines to current section
                    current_section.add_line(line)
    
    def get_section(self, name: str) -> ConfigSection:
        """Get a section by name (case-insensitive)."""
        return self.sections.get(name.lower())
    
    def get_environment_replacements(self) -> List[Tuple[str, str]]:
        """Parse environment section and return list of (pattern, replacement) tuples.
        
        Supports two formats:
        1. pattern --> replacement : Direct pattern/replacement pairs
        2. name=value : Creates pattern 'name="[^"]*"' with replacement 'name="value"'
        """
        replacements = []
        env_section = self.get_section('environment')
        
        if env_section:
            for line in env_section.lines:
                # Check for pattern --> replacement format
                if ' --> ' in line:
                    parts = line.split(' --> ', 1)
                    pattern = parts[0].strip()
                    replacement = parts[1].strip()
                    replacements.append((pattern, replacement))
                else:
                    # Parse name=value assignments
                    match = re.match(r'^([^=]+)=(.*)$', line)
                    if match:
                        name = match.group(1).strip()
                        value = match.group(2).strip()
                        # Create pattern and replacement for XML attribute format
                        pattern = rf'{re.escape(name)}="[^"]*"'
                        replacement = f'{name}="{value}"'
                        replacements.append((pattern, replacement))
        
        return replacements
    
    def get_config_vars(self) -> Dict[str, str]:
        """Extract name=value pairs from config section.
        
        Returns:
            Dictionary of configuration variable names and values
        """
        config_vars = {}
        config_section = self.get_section('config')
        
        if config_section:
            for line in config_section.lines:
                # Skip pattern --> replacement format
                if ' --> ' in line:
                    continue
                # Parse name=value assignments
                match = re.match(r'^([^=]+)=(.*)$', line)
                if match:
                    name = match.group(1).strip()
                    value = match.group(2).strip()
                    config_vars[name] = value
        
        return config_vars
    
    def get_prepare_commands(self) -> List[str]:
        """Get list of commands from prepare section."""
        prepare_section = self.get_section('prepare')
        return prepare_section.lines if prepare_section else []
    
    def get_options(self) -> Dict[str, str]:
        """Get options from options section as name=value pairs."""
        options = {}
        options_section = self.get_section('options')
        
        if options_section:
            for line in options_section.lines:
                # Parse name=value assignments
                match = re.match(r'^([^=]+)=(.*)$', line)
                if match:
                    name = match.group(1).strip()
                    value = match.group(2).strip()
                    options[name] = value
        
        return options
    
    def get_tests(self) -> List[Tuple[str, str]]:
        """Get list of tests as (name, options) tuples."""
        tests = []
        tests_section = self.get_section('tests')
        
        if tests_section:
            for line in tests_section.lines:
                # Parse "name: options" format
                match = re.match(r'^([^:]+):(.*)$', line)
                if match:
                    name = match.group(1).strip()
                    options = match.group(2).strip()
                    tests.append((name, options))
        
        return tests


class XMLProcessor:
    """Processes XML files and replaces environment variables."""
    
    @staticmethod
    def replace_variables(input_file: str, output_file: str, replacements: List[Tuple[str, str]]):
        """
        Replace patterns in XML file with replacement strings.
        
        Args:
            input_file: Path to input XML file
            output_file: Path to output XML file
            replacements: List of (pattern, replacement) tuples to apply
        """
        with open(input_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Apply each replacement
        for pattern, replacement in replacements:
            print(f"    Replacing: {pattern} --> {replacement}")
            content = re.sub(pattern, replacement, content)
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(content)


class DeploymentManager:
    """Manages deployment of environment configuration to remote machines."""
    
    def __init__(self, dry_run: bool = False, initd_path: str = '/etc/init.d', 
                 env_path: str = '/etc/HPCCSystems/environment.xml'):
        self.dry_run = dry_run
        self.initd_path = initd_path
        self.env_path = env_path
    
    def _execute_command(self, command: List[str], description: str) -> bool:
        """
        Execute a command or print it if in dry-run mode.
        
        Args:
            command: List of command arguments
            description: Human-readable description of the command
        
        Returns:
            True if successful (or dry-run), False otherwise
        """
        cmd_str = ' '.join(command)
        
        if self.dry_run:
            print(f"  [DRY-RUN] {description}")
            print(f"    Command: {cmd_str}")
            return True
        else:
            print(f"  {description}")
            print(f"    Executing: {cmd_str}")
            try:
                result = subprocess.run(command, check=True, capture_output=True, text=True)
                if result.stdout:
                    print(f"    Output: {result.stdout.strip()}")
                return True
            except subprocess.CalledProcessError as e:
                print(f"    Error: {e.stderr.strip()}", file=sys.stderr)
                return False
    
    def stop_services(self, ip_addresses: List[str]) -> bool:
        """
        Stop HPCC and Dafilesrv services on remote machines.
        
        Args:
            ip_addresses: List of IP addresses to stop services on
        
        Returns:
            True if all operations succeed, False otherwise
        """
        print("\nStopping services on remote machines...")
        
        for ip in ip_addresses:
            # Stop hpcc-init
            cmd = ['ssh', ip, f'{self.initd_path}/hpcc-init stop']
            if not self._execute_command(cmd, f"Stopping hpcc-init on {ip}"):
                return False
            
            # Stop dafilesrv
            cmd = ['ssh', ip, f'{self.initd_path}/dafilesrv stop']
            if not self._execute_command(cmd, f"Stopping dafilesrv on {ip}"):
                return False
        
        return True
    
    def copy_environment(self, source_file: str, ip_addresses: List[str]) -> bool:
        """
        Copy environment.xml to remote machines.
        
        Args:
            source_file: Local path to environment.xml
            ip_addresses: List of IP addresses to copy to
        
        Returns:
            True if all operations succeed, False otherwise
        """
        print(f"\nCopying {source_file} to remote machines...")
        
        for ip in ip_addresses:
            cmd = ['scp', source_file, f'{ip}:{self.env_path}']
            if not self._execute_command(cmd, f"Copying environment.xml to {ip}:{self.env_path}"):
                return False
        
        return True
    
    def execute_commands(self, commands: List[str], ip_addresses: List[str]) -> bool:
        """
        Execute custom commands on remote machines.
        
        Args:
            commands: List of commands to execute
            ip_addresses: List of IP addresses to execute commands on
        
        Returns:
            True if all operations succeed, False otherwise
        """
        if not commands:
            return True
        
        print(f"\nExecuting {len(commands)} custom command(s) on remote machines...")
        
        for ip in ip_addresses:
            for command in commands:
                cmd = ['ssh', ip, command]
                if not self._execute_command(cmd, f"Executing '{command}' on {ip}"):
                    return False
        
        return True
    
    def start_services(self, ip_addresses: List[str]) -> bool:
        """
        Start HPCC services on remote machines.
        
        Args:
            ip_addresses: List of IP addresses to start services on
        
        Returns:
            True if all operations succeed, False otherwise
        """
        print("\nStarting services on remote machines...")
        
        for ip in ip_addresses:
            cmd = ['ssh', ip, f'{self.initd_path}/hpcc-init start']
            if not self._execute_command(cmd, f"Starting hpcc-init on {ip}"):
                return False
        
        return True
    
    def deploy(self, environment_file: str, ip_addresses: List[str], commands: List[str] = None) -> bool:
        """
        Deploy environment configuration to remote machines.
        
        Args:
            environment_file: Path to environment.xml file
            ip_addresses: List of IP addresses to deploy to
            commands: Optional list of commands to execute before starting services
        
        Returns:
            True if deployment succeeds, False otherwise
        """
        if not ip_addresses:
            print("Warning: No IP addresses specified for deployment")
            return False
        
        if commands is None:
            commands = []
        
        print(f"\n{'='*60}")
        print(f"Deploying environment to {len(ip_addresses)} machine(s)")
        print(f"{'='*60}")
        
        # Stop services
        if not self.stop_services(ip_addresses):
            print("\nDeployment failed: Could not stop services", file=sys.stderr)
            return False
        
        # Copy environment file
        if not self.copy_environment(environment_file, ip_addresses):
            print("\nDeployment failed: Could not copy environment file", file=sys.stderr)
            return False
        
        # Execute custom commands
        if not self.execute_commands(commands, ip_addresses):
            print("\nDeployment failed: Could not execute custom commands", file=sys.stderr)
            return False
        
        # Start services
        if not self.start_services(ip_addresses):
            print("\nDeployment failed: Could not start services", file=sys.stderr)
            return False
        
        print(f"\n{'='*60}")
        print("Deployment completed successfully")
        print(f"{'='*60}")
        return True


class TestRunner:
    """Manages test execution with testsocket."""
    
    def __init__(self, dry_run: bool = False):
        self.dry_run = dry_run
    
    def _run_testsocket(self, testsocket_path: str, args: List[str], 
                       output_file: str = None) -> str:
        """
        Run testsocket command and optionally redirect output.
        
        Args:
            testsocket_path: Path to testsocket executable
            args: Arguments to pass to testsocket
            output_file: Optional file to redirect output to
        
        Returns:
            Command output as string
        """
        cmd = [testsocket_path] + args
        cmd_str = ' '.join(cmd)
        
        if output_file:
            cmd_str += f" > {output_file}"
        
        if self.dry_run:
            print(f"  [DRY-RUN] Would execute: {cmd_str}")
            return ""
        
        print(f"  Executing: {cmd_str}")
        
        try:
            if output_file:
                with open(output_file, 'w') as f:
                    result = subprocess.run(cmd, stdout=f, stderr=subprocess.PIPE, text=True)
                    if result.returncode != 0:
                        print(f"    Error: {result.stderr.strip()}", file=sys.stderr)
                return ""
            else:
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                return result.stdout
        except subprocess.CalledProcessError as e:
            print(f"    Error: {e.stderr.strip()}", file=sys.stderr)
            return ""
        except Exception as e:
            print(f"    Error: {e}", file=sys.stderr)
            return ""
    
    def _extract_filename_from_xml(self, xml_output: str) -> str:
        """
        Extract filename from XML output between <filename> tags.
        
        Args:
            xml_output: XML output containing filename tags
        
        Returns:
            Extracted filename or empty string
        """
        match = re.search(r'<filename>([^<]+)</filename>', xml_output)
        if match:
            return match.group(1)
        return ""
    
    def run_tests(self, tests: List[Tuple[str, str]], config_vars: Dict[str, str],
                  options: Dict[str, str], test_file_basename: str) -> bool:
        """
        Run tests using testsocket.
        
        Args:
            tests: List of (test_name, test_options) tuples
            config_vars: Configuration variables from [config] section
            options: Options from [options] section
            test_file_basename: Base name of the test file
        
        Returns:
            True if all tests succeed, False otherwise
        """
        if not tests:
            return True
        
        # Get configuration values with defaults
        localroot = config_vars.get('LOCALROOT', '')
        testsocket = f"{localroot}/opt/HPCCSystems/bin/testsocket" if localroot else '/opt/HPCCSystems/bin/testsocket'
        submit_ip = config_vars.get('SUBMIT_IP', '')
        roxie_port = config_vars.get('ROXIE_PORT', '')
        roxie_ips = config_vars.get('ROXIE_IPS', '')
        testfile = config_vars.get('TESTFILE', '')
        
        if not submit_ip or not roxie_port:
            print("Warning: SUBMIT_IP or ROXIE_PORT not configured")
            return False
        
        roxie_ips_list = [ip.strip() for ip in roxie_ips.split(',')] if roxie_ips else []
        event_trace = options.get('eventTrace', '0') == '1'
        
        print(f"\n{'='*60}")
        print(f"Running {len(tests)} test(s)")
        print(f"{'='*60}")
        
        for test_name, test_options in tests:
            full_test_name = f"{test_file_basename}_{test_name}"
            print(f"\nTest: {full_test_name}")
            
            # a) Start event recording if enabled
            if event_trace and roxie_ips_list:
                print("  Starting event recording...")
                for ip in roxie_ips_list:
                    event_args = [f"{ip}:{roxie_port}"]
                    if int(roxie_port) > 9999:
                        event_args.append('-ssl')
                    event_args.append("<control:startEventRecording options='all'/>")
                    self._run_testsocket(testsocket, event_args)
            
            # b) Run the actual test
            print("  Running test...")
            test_args = [f"{submit_ip}:{roxie_port}"]
            
            # Add SSL flag if port > 9999
            if int(roxie_port) > 9999:
                test_args.append('-ssl')
            
            # Add test options
            if test_options.strip():
                test_args.extend(test_options.split())
            
            # Add test file
            if testfile:
                test_args.extend(['-ff', testfile])
            
            # Add extra parameters
            test_args.extend(['-ts', '-q'])
            
            # Run test and redirect to results file
            results_file = f"results_{full_test_name}"
            self._run_testsocket(testsocket, test_args, results_file)
            print(f"  Results written to: {results_file}")
            
            # c) Stop event recording if enabled
            if event_trace and roxie_ips_list:
                print("  Stopping event recording...")
                for ip in roxie_ips_list:
                    event_args = [f"{ip}:{roxie_port}"]
                    if int(roxie_port) > 9999:
                        event_args.append('-ssl')
                    event_args.append("<control:stopEventRecording/>")
                    output = self._run_testsocket(testsocket, event_args)
                    if output:
                        filename = self._extract_filename_from_xml(output)
                        if filename:
                            print(f"    Event trace file on {ip}: {filename}")
        
        print(f"\n{'='*60}")
        print("Test execution completed")
        print(f"{'='*60}")
        return True


def parse_arguments():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description='Process configuration files and apply environment changes to XML files.'
    )
    
    parser.add_argument(
        'config_files',
        nargs='+',
        help='List of configuration files to process'
    )
    
    parser.add_argument(
        '--roxie-ips',
        default=os.environ.get('ROXIE_IPS', ''),
        help='Comma-separated list of Roxie node IPs (default: from environment)'
    )
    
    parser.add_argument(
        '--submit-ip',
        default=os.environ.get('SUBMIT_IP', ''),
        help='IP address used to submit queries to Roxie (default: from environment)'
    )
    
    parser.add_argument(
        '--roxie-port',
        default=os.environ.get('ROXIE_PORT', ''),
        help='Port number associated with Roxies (default: from environment)'
    )
    
    parser.add_argument(
        '--input-xml',
        default=None,
        help='Input XML file to process (default: from [config] INPUT or environment.in.xml)'
    )
    
    parser.add_argument(
        '--output-xml',
        default='environment.out.xml',
        help='Output XML file to write (default: environment.out.xml)'
    )
    
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Trace the deployment steps without executing them'
    )
    
    parser.add_argument(
        '--deploy',
        action='store_true',
        help='Deploy the environment configuration to remote machines'
    )
    
    return parser.parse_args()


def build_config_value(name: str, system_env: Dict[str, str], 
                       defaults_vars: Dict[str, str], config_vars: Dict[str, str], 
                       cmd_args: argparse.Namespace) -> str:
    """
    Build configuration value with proper precedence.
    
    Precedence order (highest to lowest):
    1. Command-line arguments
    2. Configuration file [config] section
    3. testing.defaults [config] section
    4. System environment variables
    
    Args:
        name: Name of the configuration variable
        system_env: System environment variables
        defaults_vars: testing.defaults file [config] section variables
        config_vars: Configuration file [config] section variables
        cmd_args: Parsed command-line arguments
    
    Returns:
        Configuration value or empty string if not found
    """
    # Check command-line arguments (highest precedence)
    arg_name = name.lower().replace('_', '-')
    cmd_value = getattr(cmd_args, arg_name.replace('-', '_'), '')
    if cmd_value:
        return cmd_value
    
    # Check config file [config] section
    if name in config_vars:
        return config_vars[name]
    
    # Check testing.defaults [config] section
    if name in defaults_vars:
        return defaults_vars[name]
    
    # Check system environment variables (lowest precedence)
    if name in system_env:
        return system_env[name]
    
    return ''


def main():
    """Main entry point for the script."""
    args = parse_arguments()
    
    # Capture system environment variables
    system_env = {
        'ROXIE_IPS': os.environ.get('ROXIE_IPS', ''),
        'SUBMIT_IP': os.environ.get('SUBMIT_IP', ''),
        'ROXIE_PORT': os.environ.get('ROXIE_PORT', '')
    }
    
    # Load testing.defaults file if it exists
    defaults_vars = {}
    defaults_path = 'testing.defaults'
    if os.path.exists(defaults_path):
        print(f"Loading defaults from: {defaults_path}")
        try:
            defaults_config = ConfigFile(defaults_path)
            defaults_vars = defaults_config.get_config_vars()
            if defaults_vars:
                print("  Default configuration values:")
                for name, value in defaults_vars.items():
                    print(f"    {name} = {value}")
        except Exception as e:
            print(f"  Warning: Could not load defaults file: {e}", file=sys.stderr)
    print()
    
    # Process each configuration file
    for config_path in args.config_files:
        print(f"Processing configuration file: {config_path}")
        
        try:
            config = ConfigFile(config_path)
            print(f"  Basename: {config.basename}")
            
            # Get configuration variables from [config] section
            config_vars = config.get_config_vars()
            
            # Build merged configuration dictionary with proper precedence
            # Start with defaults, then overlay config file values
            merged_config_vars = {}
            merged_config_vars.update(defaults_vars)
            merged_config_vars.update(config_vars)
            
            # Build final configuration values with proper precedence
            # Precedence: command-line > [config] section > testing.defaults > system environment
            roxie_ips = build_config_value('ROXIE_IPS', system_env, defaults_vars, config_vars, args)
            submit_ip = build_config_value('SUBMIT_IP', system_env, defaults_vars, config_vars, args)
            roxie_port = build_config_value('ROXIE_PORT', system_env, defaults_vars, config_vars, args)
            
            # Determine input file: command-line > [config] INPUT > defaults INPUT > default
            input_xml = args.input_xml
            if not input_xml:
                input_xml = config_vars.get('INPUT') or defaults_vars.get('INPUT', 'environment.in.xml')
            
            print(f"\n  Configuration values (after precedence resolution):")
            print(f"    INPUT = {input_xml}")
            if roxie_ips:
                print(f"    ROXIE_IPS = {roxie_ips}")
            if submit_ip:
                print(f"    SUBMIT_IP = {submit_ip}")
            if roxie_port:
                print(f"    ROXIE_PORT = {roxie_port}")
            
            # Parse ROXIE_IPS list for deployment
            roxie_ips_list = []
            if roxie_ips:
                roxie_ips_list = [ip.strip() for ip in roxie_ips.split(',')]
            
            # Get environment replacements from [environment] section only
            replacements = config.get_environment_replacements()
            print(f"\n  Found {len(replacements)} replacement(s) from [environment] section")
            
            # Display the replacements
            if replacements:
                print("\n  Replacements:")
                for pattern, replacement in replacements:
                    print(f"    {pattern} --> {replacement}")
            
            # Get prepare commands
            commands = config.get_prepare_commands()
            if commands:
                print(f"\n  Found {len(commands)} prepare command(s):")
                for cmd in commands:
                    print(f"    {cmd}")
            
            # Get options
            options = config.get_options()
            if options:
                print(f"\n  Found {len(options)} option(s):")
                for name, value in options.items():
                    print(f"    {name} = {value}")
            
            # Get tests
            tests = config.get_tests()
            if tests:
                print(f"\n  Found {len(tests)} test(s):")
                for test_name, test_opts in tests:
                    print(f"    {test_name}: {test_opts}")
            
            # Process XML file if it exists
            if os.path.exists(input_xml):
                print(f"\n  Applying replacements to {input_xml}...")
                XMLProcessor.replace_variables(
                    input_xml,
                    args.output_xml,
                    replacements
                )
                print(f"  Output written to: {args.output_xml}")
                
                # Deploy if requested
                if args.deploy and replacements:
                    remoteroot = merged_config_vars.get('REMOTEROOT', '')
                    initd_path = f"{remoteroot}/etc/init.d" if remoteroot else '/etc/init.d'
                    env_path = f"{remoteroot}/etc/HPCCSystems/environment.xml" if remoteroot else '/etc/HPCCSystems/environment.xml'
                    deployment = DeploymentManager(dry_run=args.dry_run, initd_path=initd_path, env_path=env_path)
                    if not deployment.deploy(args.output_xml, roxie_ips_list, commands):
                        sys.exit(1)
                    
                    # Run tests if deployment succeeded and tests are defined
                    if tests:
                        test_runner = TestRunner(dry_run=args.dry_run)
                        if not test_runner.run_tests(tests, merged_config_vars, options, config.basename):
                            sys.exit(1)
                elif args.deploy and not replacements:
                    print("\n  Skipping deployment: No environment replacements defined")
                    
                    # Run tests even without deployment if tests are defined
                    if tests:
                        test_runner = TestRunner(dry_run=args.dry_run)
                        if not test_runner.run_tests(tests, merged_config_vars, options, config.basename):
                            sys.exit(1)
            else:
                print(f"\n  Warning: Input XML file not found: {input_xml}")
            
            print()
            
        except FileNotFoundError:
            print(f"  Error: Configuration file not found: {config_path}", file=sys.stderr)
            sys.exit(1)
        except Exception as e:
            print(f"  Error processing {config_path}: {e}", file=sys.stderr)
            sys.exit(1)
    
    print("Processing complete.")


if __name__ == '__main__':
    main()
