#!/usr/bin/env python3
"""
Simple script to test the EchoServer functionality
"""
import socket
import sys
import time

def test_echo_server(host, port, message="Hello from client"):
    """Test the echo server by sending a message and receiving the echo"""
    try:
        # Create a socket
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.settimeout(10)  # 10 second timeout
        
        print(f"Connecting to {host}:{port}...")
        client_socket.connect((host, port))
        print("Connected successfully!")
        
        # Send test message
        print(f"Sending: '{message}'")
        client_socket.send(message.encode('utf-8'))
        
        # Receive echo
        response = client_socket.recv(1024).decode('utf-8')
        print(f"Received: '{response}'")
        
        # Check if echo matches
        if response == message:
            print("✓ Echo test PASSED - Server echoed the message correctly")
            return True
        else:
            print("✗ Echo test FAILED - Server response doesn't match sent message")
            return False
            
    except socket.timeout:
        print("✗ Connection timed out")
        return False
    except ConnectionRefusedError:
        print("✗ Connection refused - Server may not be running or port blocked")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False
    finally:
        try:
            client_socket.close()
        except:
            pass

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_echo.py <host> [port] [message]")
        print("Example: python test_echo.py 10.0.0.2")
        print("Example: python test_echo.py 10.0.0.2 7777 'Test message'")
        sys.exit(1)
    
    host = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 7777
    message = sys.argv[3] if len(sys.argv) > 3 else "Hello from test client"
    
    print(f"Testing EchoServer at {host}:{port}")
    print("-" * 50)
    
    success = test_echo_server(host, port, message)
    
    if success:
        print("\n🎉 EchoServer is working correctly!")
    else:
        print("\n❌ EchoServer test failed!")
        sys.exit(1)
