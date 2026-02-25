# Configuration File Processor

This Python script processes configuration files with multiple sections and applies environment variable replacements to XML files.

## Configuration File Format

Configuration files should contain multiple sections:

### [environment]
Contains replacements to be applied to the XML file. Supports two formats:
- `name=value` - Creates XML attribute pattern replacements
- `pattern --> replacement` - Direct regex pattern/replacement pairs

### [config]
Contains name=value assignments for configuration variables. These values override system 
environment variables but can be overridden by command-line arguments.

Supported variables:
- `ROXIE_IPS` - Comma-separated list of Roxie node IP addresses
- `SUBMIT_IP` - IP address used to submit queries to Roxie
- `ROXIE_PORT` - Port number associated with Roxies
- `INPUT` - Input XML file path (overrides default if --input-xml not specified)
- `TESTSOCKET` - Path to testsocket executable (default: /opt/HPCCSystems/bin/testsocket)
- `TESTFILE` - Test file to pass to testsocket with -ff option

### [prepare]
Contains a list of commands, one per line. These commands will be executed on each remote node 
after copying the environment file but before starting HPCC services.

### [options]
Contains name=value assignments for additional options.

Supported options:
- `eventTrace` - Set to 1 to enable event recording during tests (default: 0)
- `ssl` - Can be set to true in test options to enable SSL for testsocket

### [tests]
Contains test definitions in the format "name: options". Each test will be executed after 
successful deployment using testsocket. Test options are passed directly to testsocket.

Example:
```
basic_test: --timeout=30 --verify
ssl_test: ssl=true --verify
```

## Usage

```bash
python config_processor.py [OPTIONS] config_file1 [config_file2 ...]
```

## Options

- `--roxie-ips`: Comma-separated list of Roxie node IPs (overrides environment)
- `--submit-ip`: IP address used to submit queries to Roxie (overrides environment)
- `--roxie-port`: Port number associated with Roxies (overrides environment)
- `--input-xml`: Input XML file to process (overrides [config] INPUT or defaults to environment.in.xml)
- `--output-xml`: Output XML file to write (default: `environment.out.xml`)
- `--dry-run`: Trace the deployment and test execution steps without executing them
- `--deploy`: Deploy the environment configuration to remote machines

## Example

```bash
# Using default environment values from config file
python config_processor.py sample_config.txt

# Overriding specific values from command line
python config_processor.py --roxie-ips=10.0.0.1,10.0.0.2 --roxie-port=9999 sample_config.txt

# Processing multiple config files
python config_processor.py config1.txt config2.txt config3.txt

# Deploying to remote machines (dry-run mode)
python config_processor.py --deploy --dry-run sample_config.txt

# Deploying to remote machines (actual deployment)
python config_processor.py --deploy sample_config.txt
```

## Sample Configuration File

```
[environment]
ROXIE_IPS=192.168.1.10,192.168.1.11,192.168.1.12
SUBMIT_IP=192.168.1.10
ROXIE_PORT=9876
TEST_VAR=sample_value
old_server_name --> new_server_name
192\.168\.0\.\d+ --> 10.0.0.1

[prepare]
start_roxie_cluster
check_cluster_health
deploy_queries

[tests]
basic_test: --timeout=30 --verify
performance_test: --duration=60 --load=high
smoke_test: --quick
```

Note: The environment section supports two formats:
- `name=value` - Creates XML attribute pattern replacements
- `pattern --> replacement` - Direct regex pattern/replacement pairs

## How It Works

1. The script reads each configuration file and parses it into sections
2. Configuration values (ROXIE_IPS, SUBMIT_IP, ROXIE_PORT) are determined using the following precedence order (highest to lowest):
   - **Command-line arguments** (highest priority)
   - **Configuration file `[config]` section**
   - **System environment variables** (lowest priority)
3. It extracts pattern replacements from the `[environment]` section
4. The script reads the input XML file and applies all pattern replacements
5. The modified XML is written to the output file
6. If `--deploy` is specified, the script will:
   - Stop HPCC services (`/etc/init.d/hpcc-init stop`) on all Roxie nodes
   - Stop Dafilesrv (`/etc/init.d/dafilesrv stop`) on all Roxie nodes
   - Copy the output XML to `/etc/HPCCSystems/environment.xml` on each node
   - Execute each command from the `[prepare]` section on each node
   - Start HPCC services (`/etc/init.d/hpcc-init start`) on all Roxie nodes
7. If tests are defined in the `[tests]` section, they will be executed:
   - If `eventTrace=1` in `[options]`, start event recording on all Roxie nodes
   - Run the test using testsocket with specified options
   - Results are saved to `results_{basename}_{testname}` files
   - If `eventTrace=1`, stop event recording and display trace file locations

**Note**: The deployment assumes SSH keys are pre-configured for passwordless authentication.

## Precedence Example

Given:
- System environment: `ROXIE_IPS=192.168.0.1,192.168.0.2`
- Config file `[config]` section: `ROXIE_IPS=192.168.1.10,192.168.1.11`
- Command line: `--roxie-ips=10.0.0.1,10.0.0.2`

The final value will be: `10.0.0.1,10.0.0.2` (command line wins)

## Dry-Run Mode

Use `--dry-run` to see what commands would be executed without actually running them:

```bash
python config_processor.py --deploy --dry-run sample_config.txt
```

This will trace all SSH and SCP commands that would be executed during deployment.

## Future Enhancements

- Implementation of command execution stage
- Implementation of test execution stage
