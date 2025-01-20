import sys
import json
import traceback
import argparse
from io import StringIO
import threading
import queue


def parse_arguments():
    parser = argparse.ArgumentParser(description="Python Worker Script")
    parser.add_argument('--token', required=True, help='Authentication token for executing scripts.')
    return parser.parse_args()


def execute_script(token, SECRET_TOKEN, data, result_queue):
    try:
        # Verify secret token
        received_token = data.get("token", "")
        if received_token != SECRET_TOKEN:
            raise PermissionError("Invalid authentication token.")

        script = data.get("script", "")
        arguments = data.get("arguments", [])

        # Redirect stdout and stderr
        old_stdout = sys.stdout
        old_stderr = sys.stderr
        sys.stdout = StringIO()
        sys.stderr = StringIO()

        # Execute the script
        exec_globals = {}
        for i, arg in enumerate(arguments):
            exec_globals[f'arg{i+1}'] = arg
        exec(script, exec_globals)

        # Capture output
        output = sys.stdout.getvalue()
        error_output = sys.stderr.getvalue()

        # Restore stdout and stderr
        sys.stdout = old_stdout
        sys.stderr = old_stderr

        # Prepare result
        result = {
            "success": True,
            "output": output,
            "error": error_output
        }
    except Exception as e:
        # Capture traceback
        error_output = traceback.format_exc()
        result = {
            "success": False,
            "output": "",
            "error": error_output
        }

    # Put the result in the queue
    result_queue.put(result)


def control_listener(command_queue, shutdown_event):
    """
    Listens for control commands from the main thread.
    """
    while not shutdown_event.is_set():
        try:
            # Wait for a command with a timeout to allow checking the shutdown_event
            command = command_queue.get(timeout=0.5)
            if command == "shutdown":
                shutdown_event.set()
                break
            elif command.startswith("update_config"):
                # Handle configuration update
                # Example: "update_config:<new_config_json>"
                _, new_config_json = command.split(":", 1)
                new_config = json.loads(new_config_json)
                # Update configuration as needed
                # For simplicity, we're not implementing actual config handling here
                print(json.dumps({"success": True, "message": "Configuration updated."}))
            else:
                print(json.dumps({"success": False, "error": "Unknown control command."}))
        except queue.Empty:
            continue
        except Exception as e:
            print(json.dumps({"success": False, "error": str(e)}))


def main():
    args = parse_arguments()
    SECRET_TOKEN = args.token  # The token provided via command line

    # Queues for inter-thread communication
    result_queue = queue.Queue()
    command_queue = queue.Queue()
    shutdown_event = threading.Event()

    # Start the control listener thread
    control_thread = threading.Thread(target=control_listener, args=(command_queue, shutdown_event))
    control_thread.start()

    try:
        # Read input from stdin line by line to handle multiple commands
        for line in sys.stdin:
            if shutdown_event.is_set():
                break
            if not line.strip():
                continue

            data = json.loads(line)

            if data.get("command") == "execute":
                # Handle script execution in a separate thread
                exec_thread = threading.Thread(target=execute_script, args=(data.get("token"), SECRET_TOKEN, data, result_queue))
                exec_thread.start()
                exec_thread.join()  # Wait for execution to finish

                # Retrieve and print the result
                result = result_queue.get()
                print(json.dumps(result))
                sys.stdout.flush()
            elif data.get("command") == "shutdown":
                # Signal the control thread to shutdown
                shutdown_event.set()
                break
            else:
                # Unknown command
                response = {"success": False, "error": "Unknown command."}
                print(json.dumps(response))
                sys.stdout.flush()
    except Exception as e:
        error_output = traceback.format_exc()
        result = {"success": False, "output": "", "error": error_output}
        print(json.dumps(result))
    finally:
        # Ensure the control thread is properly shutdown
        shutdown_event.set()
        control_thread.join()


if __name__ == "__main__":
    main()
