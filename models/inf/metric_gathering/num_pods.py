from kubernetes import client, config, watch
import time
import signal
import sys


POD_NAME_PREFIX = "resnet50v2tf-predictor-default-00001-deployment-"


class PodLogEntry:
    def __init__(self, timestamp, running, pending, terminating):
        self.timestamp = timestamp
        self.running = running
        self.pending = pending
        self.terminating = terminating

pods = {}
pod_log = []


class StampedInt:
    def __init__(self, timestamp, int):
        self.timestamp = timestamp
        self.int = int

def write_stamped_ints_to_file(stamped_ints, file_name):
    with open(file_name, 'w') as f:
        for si in stamped_ints:
            f.write(f'{si.timestamp:.9f} {si.int}\n')


# Write the log to files
def signal_handler(signal, frame):
    running_pods = []
    pending_pods = []
    terminating_pods = []
    for log_entry in pod_log:
        running_pods.append(StampedInt(log_entry.timestamp, log_entry.running))
        pending_pods.append(StampedInt(log_entry.timestamp, log_entry.pending))
        terminating_pods.append(StampedInt(log_entry.timestamp, log_entry.terminating))

    write_stamped_ints_to_file(running_pods, "running_pods.txt")
    write_stamped_ints_to_file(pending_pods, "pending_pods.txt")
    write_stamped_ints_to_file(terminating_pods, "terminating_pods.txt")

    sys.exit(0)


# Log number of Pending, Running, and Terminating pods
def log_pod_info(event_time):
    num_pending = 0
    num_running = 0
    num_terminating = 0

    for pod_status in pods.values():
        if pod_status == "Pending":
            num_pending += 1
        elif pod_status == "Running":
            num_running += 1
        elif pod_status == "Terminating":
            num_terminating += 1
    
    global pod_log
    pod_log.append(PodLogEntry(event_time, num_running, num_pending, num_terminating))


def handle_pod_event(event):
    event_time = time.time()
    global pods

    pod = event['object']
    event_type = event['type']
    name = pod.metadata.name
    status = pod.status.phase

    deletion_timestamp = pod.metadata.deletion_timestamp
    if deletion_timestamp:
        status = "Terminating"
    
    if not POD_NAME_PREFIX in name:
        return
    
    id = name[name.find(POD_NAME_PREFIX) + len(POD_NAME_PREFIX):]

    # print(f"Event: {event_type}, ID: {id}, Status: {status}")

    if event_type == "ADDED":
        if id in pods:
            print("ERROR: Pod added that already exists!")
            exit(-1)
        pods[id] = status
    elif event_type == "MODIFIED":
        pods[id] = status
    elif event_type == "DELETED":
        del pods[id]
    
    log_pod_info(event_time=event_time)


def main():
    # Set up signal handler for writing to file
    signal.signal(signal.SIGINT, signal_handler)

    # Load the Kubernetes configuration
    config.load_kube_config()

    # Create an API client instance
    api_instance = client.CoreV1Api()

    # Set up a watch on pods in all namespaces
    w = watch.Watch()
    for event in w.stream(api_instance.list_pod_for_all_namespaces):
        handle_pod_event(event)

if __name__ == '__main__':
    main()