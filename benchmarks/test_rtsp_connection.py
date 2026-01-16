#!/usr/bin/env python3
"""
RTSP Connection Test Script
Tests the improved RTSP port forwarding functionality
"""

import socket
import time
import threading
import sys

def test_rtsp_connection(host, port, camera_url):
    """Test RTSP connection through port forwarder"""
    print(f"Testing RTSP connection to {host}:{port}")
    
    try:
        # Create socket with timeout
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(30)  # 30 second timeout
        
        print(f"Connecting to {host}:{port}...")
        sock.connect((host, port))
        print("✅ TCP connection established")
        
        # Send RTSP OPTIONS request
        options_request = f"OPTIONS {camera_url} RTSP/1.0\r\nCSeq: 1\r\nUser-Agent: RTSP-Test-Client\r\n\r\n"
        print(f"Sending OPTIONS request:")
        print(options_request.strip())
        
        sock.send(options_request.encode())
        
        # Wait for response
        print("Waiting for response...")
        response = sock.recv(4096)
        
        if response:
            print("✅ Received RTSP response:")
            print(response.decode('utf-8', errors='ignore'))
            
            # Check if it's a valid RTSP response
            if response.startswith(b"RTSP/1.0"):
                print("✅ Valid RTSP response received!")
                return True
            else:
                print("❌ Invalid RTSP response format")
                return False
        else:
            print("❌ No response received")
            return False
            
    except socket.timeout:
        print("❌ Connection timeout - camera might be unreachable")
        return False
    except ConnectionRefusedError:
        print("❌ Connection refused - port forwarder might not be running")
        return False
    except Exception as e:
        print(f"❌ Connection error: {e}")
        return False
    finally:
        sock.close()

def test_persistent_connection(host, port, camera_url, duration=60):
    """Test persistent RTSP connection"""
    print(f"\nTesting persistent connection for {duration} seconds...")
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(30)
        sock.connect((host, port))
        
        # Send DESCRIBE request to get stream info
        describe_request = f"DESCRIBE {camera_url} RTSP/1.0\r\nCSeq: 2\r\nAccept: application/sdp\r\nUser-Agent: RTSP-Test-Client\r\n\r\n"
        sock.send(describe_request.encode())
        
        start_time = time.time()
        total_bytes = 0
        
        while time.time() - start_time < duration:
            try:
                data = sock.recv(4096)
                if data:
                    total_bytes += len(data)
                    if data.startswith(b"RTSP/1.0"):
                        print(f"RTSP response: {data.decode('utf-8', errors='ignore')[:100]}...")
                else:
                    print("Connection closed by remote")
                    break
            except socket.timeout:
                print("No data received in 30 seconds")
                break
                
        elapsed = time.time() - start_time
        print(f"Connection lasted {elapsed:.1f} seconds, received {total_bytes} bytes")
        
        if elapsed > duration * 0.8:  # If we got at least 80% of the way
            print("✅ Persistent connection test passed")
            return True
        else:
            print("❌ Connection closed too early")
            return False
            
    except Exception as e:
        print(f"❌ Persistent connection error: {e}")
        return False
    finally:
        sock.close()

def main():
    if len(sys.argv) < 2:
        print("Usage: python test_rtsp_connection.py <port> [host] [camera_url]")
        print("Example: python test_rtsp_connection.py 8551")
        print("Example: python test_rtsp_connection.py 8551 10.0.0.2 rtsp://admin:password@10.0.0.2:8551/cam/realmonitor?channel=1&subtype=0")
        sys.exit(1)
    
    port = int(sys.argv[1])
    host = sys.argv[2] if len(sys.argv) > 2 else "10.0.0.2"
    camera_url = sys.argv[3] if len(sys.argv) > 3 else f"rtsp://admin:industry4@{host}:{port}/cam/realmonitor?channel=1&subtype=0"
    
    print("🔧 RTSP Port Forwarding Connection Test")
    print("=" * 50)
    print(f"Target: {host}:{port}")
    print(f"Camera URL: {camera_url}")
    print("=" * 50)
    
    # Test 1: Basic connection
    print("\n📡 Test 1: Basic RTSP Connection")
    success1 = test_rtsp_connection(host, port, camera_url)
    
    # Test 2: Persistent connection
    print("\n⏱️  Test 2: Persistent Connection (30 seconds)")
    success2 = test_persistent_connection(host, port, camera_url, 30)
    
    # Summary
    print("\n📊 Test Summary")
    print("=" * 30)
    print(f"Basic Connection: {'✅ PASS' if success1 else '❌ FAIL'}")
    print(f"Persistent Connection: {'✅ PASS' if success2 else '❌ FAIL'}")
    
    if success1 and success2:
        print("\n🎉 All tests passed! RTSP forwarding is working correctly.")
        sys.exit(0)
    else:
        print("\n⚠️  Some tests failed. Check the port forwarder logs for details.")
        sys.exit(1)

if __name__ == "__main__":
    main()
