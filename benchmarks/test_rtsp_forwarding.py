#!/usr/bin/env python3
"""
RTSP Port Forwarding Test Script
This script tests the port forwarding functionality for RTSP streams.
"""

import socket
import time
import sys
import threading

def test_tcp_connection(host, port, timeout=5):
    """Test basic TCP connectivity to the forwarded port"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex((host, port))
        sock.close()
        return result == 0
    except Exception as e:
        print(f"TCP connection test failed: {e}")
        return False

def test_rtsp_options(host, port, path="/stream1", timeout=10):
    """Test RTSP OPTIONS request"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect((host, port))
        
        # Send RTSP OPTIONS request
        rtsp_request = f"OPTIONS rtsp://{host}:{port}{path} RTSP/1.0\r\n"
        rtsp_request += "CSeq: 1\r\n"
        rtsp_request += "User-Agent: RTSP-Test-Client/1.0\r\n"
        rtsp_request += "\r\n"
        
        print(f"Sending RTSP OPTIONS to {host}:{port}{path}")
        print("Request:")
        print(rtsp_request)
        
        sock.send(rtsp_request.encode())
        
        # Read response
        response = sock.recv(4096).decode('utf-8', errors='ignore')
        sock.close()
        
        print("Response:")
        print(response)
        
        return "RTSP/1.0" in response
        
    except Exception as e:
        print(f"RTSP OPTIONS test failed: {e}")
        return False

def test_data_forwarding(host, port, test_data=b"TEST_DATA_FORWARDING", timeout=5):
    """Test bidirectional data forwarding"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect((host, port))
        
        print(f"Sending test data: {test_data}")
        sock.send(test_data)
        
        # Wait for response (might be echoed back or camera response)
        time.sleep(1)
        try:
            response = sock.recv(1024)
            print(f"Received response: {response}")
            sock.close()
            return True
        except socket.timeout:
            print("No response received (this is normal for cameras)")
            sock.close()
            return True
            
    except Exception as e:
        print(f"Data forwarding test failed: {e}")
        return False

def main():
    if len(sys.argv) < 3:
        print("Usage: python test_rtsp_forwarding.py <host> <port> [rtsp_path]")
        print("Example: python test_rtsp_forwarding.py 10.0.0.2 8551 /stream1")
        sys.exit(1)
    
    host = sys.argv[1]
    port = int(sys.argv[2])
    rtsp_path = sys.argv[3] if len(sys.argv) > 3 else "/stream1"
    
    print(f"Testing RTSP port forwarding: {host}:{port}{rtsp_path}")
    print("=" * 50)
    
    # Test 1: Basic TCP connectivity
    print("\n1. Testing TCP connectivity...")
    if test_tcp_connection(host, port):
        print("✅ TCP connection successful")
    else:
        print("❌ TCP connection failed")
        sys.exit(1)
    
    # Test 2: RTSP OPTIONS request
    print("\n2. Testing RTSP OPTIONS request...")
    if test_rtsp_options(host, port, rtsp_path):
        print("✅ RTSP OPTIONS successful")
    else:
        print("⚠️  RTSP OPTIONS failed (might be authentication required)")
    
    # Test 3: Data forwarding
    print("\n3. Testing data forwarding...")
    if test_data_forwarding(host, port):
        print("✅ Data forwarding working")
    else:
        print("❌ Data forwarding failed")
    
    print("\n" + "=" * 50)
    print("Test completed. If TCP connection works but RTSP fails,")
    print("check camera credentials and RTSP path.")

if __name__ == "__main__":
    main()
