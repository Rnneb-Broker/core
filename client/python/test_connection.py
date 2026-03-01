#!/usr/bin/env python3

"""
Connection Test
Simple test to verify the Python client can connect to the broker
"""

import sys
import time
import os

# Add parent directory to path to import highway_client
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from highway_client import HighwayClient, QoS

def main():
    print("=" * 60)
    print("HIGHWAY BROKER - PYTHON CLIENT CONNECTION TEST")
    print("=" * 60)
    print()
    
    connected = False
    error_occurred = False
    error_msg = None
    
    client = HighwayClient({
        'host': 'localhost',
        'port': 1883,
        'client_id': f'py-test-{int(time.time() * 1000) % 1000000}',
        'auto_connect': False  # Don't auto-connect
    })
    
    def on_connect():
        nonlocal connected
        connected = True
        print("✅ [EVENT] Connected and authenticated")
        
        # Try subscribing
        def on_subscribe_ack(result):
            print(f"✅ [SUBSCRIBE] Acknowledged: {result['granted_qos_list']}")
            
            # Publish a test message
            def on_publish_ack(success):
                print(f"✅ [PUBLISH] Message published successfully")
                
                # Wait a bit for any incoming messages
                time.sleep(1)
                client.disconnect(lambda: None)
            
            client.publish('test/topic', 'Hello from Python!', QoS.AT_LEAST_ONCE, on_publish_ack)
        
        client.subscribe('test/topic', QoS.AT_LEAST_ONCE, on_subscribe_ack)
    
    def on_message(msg):
        print(f"✅ [MESSAGE] Received: {msg}")
    
    def on_error(err):
        nonlocal error_occurred, error_msg
        error_occurred = True
        error_msg = str(err)
        print(f"❌ [ERROR] {err}")
    
    def on_close():
        print("🔌 [EVENT] Connection closed")
    
    client.on('connect', on_connect)
    client.on('message', on_message)
    client.on('error', on_error)
    client.on('close', on_close)
    
    print("[TEST] Attempting to connect to localhost:1883...")
    client.connect(lambda success, err: 
        print(f"✅ Connection established") if success 
        else print(f"❌ Connection callback: {err}"))
    
    # Wait for connection or timeout
    timeout = time.time() + 10
    while time.time() < timeout:
        if connected or error_occurred:
            break
        time.sleep(0.1)
    
    # Keep running while connected
    while client.is_connected() and time.time() < timeout:
        time.sleep(0.1)
    
    print()
    print("=" * 60)
    if connected:
        print("✅ TEST PASSED - Client connected successfully")
    elif error_occurred:
        print(f"❌ TEST FAILED - Error: {error_msg}")
    else:
        print("❌ TEST FAILED - Connection timeout")
    print("=" * 60)
    
    return 0 if connected else 1

if __name__ == '__main__':
    sys.exit(main())
