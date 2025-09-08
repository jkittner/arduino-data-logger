#!/usr/bin/env python3
"""
Precise Arduino Data Logger Time Synchronization Script
This script synchronizes time at exact second boundaries for maximum accuracy.
"""
import argparse
import datetime
import sys
import time

import serial


def find_arduino_port():
    """Try to automatically find the Arduino port"""
    import serial.tools.list_ports

    # Common Arduino USB identifiers
    arduino_identifiers = [
        'Arduino',
        'USB-SERIAL',
        'USB Serial',
        'CH340',  # Common USB-to-serial chip
        'FTDI',   # Another common USB-to-serial chip
        'ttyACM',  # Linux Arduino port prefix
        'ttyUSB',   # Linux USB serial port prefix
    ]

    ports = serial.tools.list_ports.comports()
    for port in ports:
        port_info = f"{port.device} {port.description} {port.manufacturer or ''}".upper()  # noqa: E501
        for identifier in arduino_identifiers:
            if identifier.upper() in port_info:
                return port.device

    return None


def list_serial_ports():
    """List all available serial ports"""
    import serial.tools.list_ports

    ports = serial.tools.list_ports.comports()
    if not ports:
        print('No serial ports found.')
        return

    print('Available serial ports:')
    for i, port in enumerate(ports, 1):
        print(f"  {i}. {port.device} - {port.description}")
        if port.manufacturer:
            print(f"     Manufacturer: {port.manufacturer}")


def sync_precise_time_to_arduino(port, baudrate=9600, timeout=30):
    """
    Sync time to Arduino with precise timing using handshake protocol

    Args:
        port (str): Serial port
        baudrate (int): Serial communication speed
        timeout (int): Communication timeout in seconds

    Returns:
        bool: True if sync successful, False otherwise
    """
    try:
        # Open serial connection
        print(f"Connecting to Arduino on port {port} at {baudrate} baud...")
        ser = serial.Serial(port, baudrate, timeout=timeout)

        # Wait for Arduino to initialize
        time.sleep(2)

        # Clear any existing data in the buffer
        ser.flushInput()
        ser.flushOutput()

        print('Waiting for Arduino WAKE_UP signal...')

        # Wait for Arduino to send WAKE_UP signal
        start_time = time.time()
        while time.time() < start_time + timeout:
            if ser.in_waiting > 0:
                line = ser.readline().decode().strip()
                print(f"Received: {line}")

                if line == 'WAKE_UP':
                    print('Arduino is awake! Sending precise time...')

                    # Get current time immediately when Arduino signals
                    sync_time = datetime.datetime.now(datetime.UTC)

                    # Format and send command immediately (no delay compensation needed)
                    time_command = f"SYNC_TIME:{sync_time.year},{sync_time.month:02d},{sync_time.day:02d},{sync_time.hour:02d},{sync_time.minute:02d},{sync_time.second:02d}\n"  # noqa: E501

                    print(f"Syncing to: {sync_time.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}")  # noqa: E501

                    ser.write(time_command.encode())
                    ser.flush()

                    # Wait for response
                    response_timeout = time.time() + 5
                    while time.time() < response_timeout:
                        if ser.in_waiting > 0:
                            response = ser.readline().decode().strip()
                            print(f"Arduino response: {response}")

                            if response.startswith('TIME_SYNCED:OK'):
                                print('Precise time synchronization successful!')

                                # Read the RTC confirmation line
                                confirmation_timeout = time.time() + 2
                                while time.time() < confirmation_timeout:
                                    if ser.in_waiting > 0:
                                        confirmation = ser.readline().decode().strip()
                                        if confirmation.startswith('RTC:'):
                                            print(
                                                f"Arduino confirms: {confirmation}",
                                            )
                                            break

                                ser.close()
                                return True
                            elif response.startswith('TIME_SYNCED:ERR'):
                                print('Time synchronization failed')
                                ser.close()
                                return False

                    print('Timeout waiting for sync confirmation')
                    ser.close()
                    return False

        print('Timeout waiting for Arduino WAKE_UP signal')
        print('Make sure Arduino is running and waking up within the timeout period')
        ser.close()
        return False

    except serial.SerialException as e:
        print(f"Serial communication error: {e}")
        return False
    except Exception as e:
        print(f"Unexpected error: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description='Precise Arduino time synchronization',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python sync_time_precise.py                    # Auto-detect Arduino port
  python sync_time_precise.py --port /dev/ttyACM0  # Linux
  python sync_time_precise.py --port COM3          # Windows
  python sync_time_precise.py --list-ports         # Show available ports
        """,
    )
    parser.add_argument(
        '--port', '-p',
        help='Serial port (e.g., /dev/ttyACM0, COM3)',
    )
    parser.add_argument(
        '--baudrate', '-b', type=int, default=9600,
        help='Serial baudrate (default: 9600)',
    )
    parser.add_argument(
        '--timeout', '-t', type=int, default=30,
        help='Communication timeout in seconds (default: 30)',
    )
    parser.add_argument(
        '--list-ports', '-l', action='store_true',
        help='List available serial ports and exit',
    )

    args = parser.parse_args()

    if args.list_ports:
        list_serial_ports()
        return

    print('Precise Arduino Time Synchronization')
    print('====================================')

    # Try to find Arduino port if not specified
    port = args.port
    if not port:
        print('No port specified, attempting to auto-detect Arduino...')
        port = find_arduino_port()

        if not port:
            print('Could not auto-detect Arduino port.')
            print('Please specify the port manually using --port option.')
            print('Use --list-ports to see available ports.')
            sys.exit(1)
        else:
            print(f"Auto-detected Arduino on port: {port}")

    success = sync_precise_time_to_arduino(port, args.baudrate, args.timeout)

    if success:
        print('\n✓ Precise time synchronization completed!')
    else:
        print('\n✗ Time synchronization failed!')
        sys.exit(1)


if __name__ == '__main__':
    main()
