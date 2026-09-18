import paho.mqtt.client as mqtt
import time
import json
import ssl
import statistics
import threading
import getpass
import argparse
import csv

parser = argparse.ArgumentParser()
parser.add_argument("--mode", choices=["mqtt", "mqtts"], required=True)
args = parser.parse_args()

# CONFIGURATION
BROKER = "192.168.100.65"
TOPIC = "benchmark/performance"

MESSAGE_COUNT = 200
MESSAGE_INTERVAL = 0.1

CA_FILE = r"C:\Users\PC\Documents\IoT-DT\mosquitto\config\certs\ca.crt"

if args.mode == "mqtt":
    PORT = 1883
    USE_TLS = False
else:
    PORT = 8883
    USE_TLS = True

print(f"\nBenchmark mode: {args.mode.upper()}")
print(f"Broker: {BROKER}:{PORT}")
print(f"Messages: {MESSAGE_COUNT}\n")

PUB_PASSWORD = getpass.getpass("Password for esp32_device: ")
SUB_PASSWORD = getpass.getpass("Password for ditto_service: ")

# VARIABLES
latencies = []
received_sequences = set()

subscriber_connected = threading.Event()
publisher_connected = threading.Event()
subscription_ready = threading.Event()

subscriber_connection_ms = 0
publisher_connection_ms = 0

subscriber_start = 0
publisher_start = 0

# CALLBACKS
def on_sub_connect(client, userdata, flags, reason_code, properties):
    global subscriber_connection_ms

    subscriber_connection_ms = (time.perf_counter_ns() - subscriber_start) / 1_000_000
    print(f"Subscriber connected in "f"{subscriber_connection_ms:.2f} ms")
    client.subscribe(TOPIC, qos=1)
    subscriber_connected.set()


def on_subscribe(client, userdata, mid, reason_code_list, properties):
    subscription_ready.set()

def on_message(client, userdata, msg):
    receive_time = time.perf_counter_ns()

    try:
        payload = json.loads(msg.payload.decode())
        sequence = payload["sequence"]
        send_time = payload["send_time"]
        latency_ms = (receive_time - send_time) / 1_000_000
        latencies.append(latency_ms)
        received_sequences.add(sequence)

    except Exception as e:
        print("Error:", e)


def on_pub_connect(client, userdata, flags, reason_code, properties):
    global publisher_connection_ms

    publisher_connection_ms = (time.perf_counter_ns() - publisher_start) / 1_000_000

    print(f"Publisher connected in "f"{publisher_connection_ms:.2f} ms")
    publisher_connected.set()

# CLIENTS
subscriber = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,client_id=f"benchmark_sub_{args.mode}")

publisher = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,client_id=f"benchmark_pub_{args.mode}")

# Assign callbacks
subscriber.on_connect = on_sub_connect
subscriber.on_subscribe = on_subscribe
subscriber.on_message = on_message
publisher.on_connect = on_pub_connect

subscriber.username_pw_set("ditto_service",SUB_PASSWORD)

publisher.username_pw_set("esp32_device",PUB_PASSWORD)

if USE_TLS:
    subscriber.tls_set(ca_certs=CA_FILE,cert_reqs=ssl.CERT_REQUIRED)
    publisher.tls_set(ca_certs=CA_FILE, cert_reqs=ssl.CERT_REQUIRED)

# CONNECT SUBSCRIBER
subscriber.loop_start()
subscriber_start = time.perf_counter_ns()
subscriber.connect(BROKER,PORT,keepalive=60)

if not subscriber_connected.wait(10):
    raise RuntimeError("Subscriber connection failed.")

if not subscription_ready.wait(5):
    raise RuntimeError("Subscription failed.")

# CONNECT PUBLISHER
publisher.loop_start()
publisher_start = time.perf_counter_ns()
publisher.connect(BROKER,PORT,keepalive=60)

if not publisher_connected.wait(10):
    raise RuntimeError("Publisher connection failed.")

time.sleep(1)
print("\nSending benchmark messages...\n")

# SEND MESSAGES
for sequence in range(1, MESSAGE_COUNT + 1):
    send_time = time.perf_counter_ns()

    payload = {
        "sequence": sequence,
        "send_time": send_time,
        "test": args.mode
    }
    info = publisher.publish(TOPIC,json.dumps(payload),qos=1)
    info.wait_for_publish()
    time.sleep(MESSAGE_INTERVAL)

# Allow last packets to arrive
time.sleep(3)

subscriber.disconnect()
publisher.disconnect()

subscriber.loop_stop()
publisher.loop_stop()

# RESULTS
received = len(received_sequences)
delivery_rate = (received / MESSAGE_COUNT) * 100

if latencies:
    average = statistics.mean(latencies)
    median = statistics.median(latencies)
    minimum = min(latencies)
    maximum = max(latencies)
    stdev = statistics.stdev(latencies)
    sorted_latency = sorted(latencies)
    p95_index = int(0.95 * len(sorted_latency)) - 1
    p95 = sorted_latency[p95_index]

    #BENCHMARK RESULTS
    print("\n")
    print("STATISTICAL METRICS")
    print(f"Mode: {args.mode.upper()}")
    print(f"Sent: {MESSAGE_COUNT}")
    print(f"Received: {received}")
    print(f"Delivery: {delivery_rate:.2f}%")
    print(f"Publisher connection: "f"{publisher_connection_ms:.2f} ms")
    print(f"Subscriber connection: "f"{subscriber_connection_ms:.2f} ms")
    print(f"Mean latency: {average:.3f} ms")
    print(f"Median latency: {median:.3f} ms")
    print(f"Minimum: {minimum:.3f} ms")
    print(f"Maximum: {maximum:.3f} ms")
    print(f"95th percentile: {p95:.3f} ms")
    print(f"Std deviation: {stdev:.3f} ms")

    filename = f"{args.mode}_results.csv"

    with open(filename,"w",newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["Measurement", "Value"])
        writer.writerow(["Messages sent", MESSAGE_COUNT])
        writer.writerow(["Messages received", received])
        writer.writerow(["Delivery rate (%)", delivery_rate])
        writer.writerow(["Publisher connection (ms)",publisher_connection_ms])
        writer.writerow(["Subscriber connection (ms)",subscriber_connection_ms])
        writer.writerow(["Mean latency (ms)", average])
        writer.writerow(["Median latency (ms)", median])
        writer.writerow(["Minimum latency (ms)", minimum])
        writer.writerow(["Maximum latency (ms)", maximum])
        writer.writerow(["95th percentile (ms)", p95])
        writer.writerow(["Standard deviation (ms)", stdev])

    print(f"\nResults saved to: {filename}")