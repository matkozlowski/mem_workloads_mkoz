import requests
import base64
import argparse
import json
import numpy as np
from PIL import Image
import time
import asyncio
import aiohttp
from tqdm import tqdm


actual_delays = []
last_task_start_time = 0
actual_delay_filename = "observed_delays.txt"
latencies_filename = "latencies.txt"
times_filename = "request_script_times.txt"

class StampedLatency:
    def __init__(self, timestamp, latency):
        self.timestamp = timestamp
        self.latency = latency

def write_floats_to_file(floats, file_name):
    with open(file_name, 'w') as f:
        for num in floats:
            f.write(f'{num:.9f}\n')

def write_stamped_latencies_to_file(stamped_latencies, file_name):
    with open(file_name, 'w') as f:
        for sl in stamped_latencies:
            f.write(f'{sl.timestamp:.9f} {sl.latency:.9f}\n')

def write_current_time_to_file(file_name):
    current_time_seconds = time.time()
    with open(file_name, 'a') as f:
        f.write(f'{current_time_seconds:.2f}\n')

def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ingress_host", required=True)
    parser.add_argument("--service_host", required=True)
    parser.add_argument("--model_name", required=True)
    parser.add_argument("--trace_file", required=True)
    parser.add_argument("--delay_scale", required=True, type=float)
    parser.add_argument("--num_trace_reads", required=False, type=int)
    #parser.add_argument("--average_delay", required=True, type=float, help="average time [in seconds, floating-point] to wait before sending a request")
    return parser.parse_args()

def get_normalized_delays(args):
    delays = []
    with open(args.trace_file) as f:
        for line_num, line in enumerate(f):
            if args.num_trace_reads and line_num >= args.num_trace_reads:
                break
            delays.append(float(line.strip()))

    # avg_trace_val = 0
    # for d in delays:
    #     avg_trace_val = avg_trace_val + d
    # avg_trace_val = avg_trace_val / len(delays)
    
    # scale_by = args.average_delay / avg_trace_val
    scale_by = args.delay_scale

    delays = delays[scale_by - 1 :: scale_by]

    for i in range(len(delays)):
        delays[i] = delays[i] * scale_by
        
    return delays

def gen_request(args):
    jpeg_rgb = Image.open("./0005.jpg")
    jpeg_rgb = np.expand_dims(np.array(jpeg_rgb) / 255.0, 0).tolist()

    predict_request = json.dumps({'instances': jpeg_rgb})
    headers = {"Host": f"{args.service_host}", "Content-Type": "application/json"}
    url = f"http://{args.ingress_host}:80/v1/models/{args.model_name}:predict"
    return url, headers, predict_request
    
async def send_request_async(url, headers, data):
    async with aiohttp.ClientSession() as session:
        global last_task_start_time
        global actual_delays

        start_time = time.perf_counter()
        actual_delays.append(start_time - last_task_start_time)
        last_task_start_time = start_time

        async with session.post(url, headers=headers, data=data) as response:
            end_time = time.perf_counter()
            latency = (end_time - start_time) * 1000  # Convert the latency to milliseconds
            stamped_latency = StampedLatency(timestamp=start_time, latency=latency)
            return stamped_latency, await response.text()

async def main():
    args = get_args()
    delays = get_normalized_delays(args)
    url, headers, data = gen_request(args)
    
    write_current_time_to_file(times_filename)

    tasks = []
    last_task_start_time = time.perf_counter()
    for delay in tqdm(delays, desc="Sending requests"):
        await asyncio.sleep(delay) # in seconds
        task = asyncio.ensure_future(send_request_async(url, headers, data))
        tasks.append(task)
    
    write_current_time_to_file(times_filename)

    stamped_latencies, _ = zip(*await asyncio.gather(*tasks))
    
    # average_latency = sum(latencies) / len(latencies)
    # print(f"Average request latency for {len(latencies)} requests: {average_latency:.2f} ms")

    write_floats_to_file(actual_delays, actual_delay_filename)
    write_stamped_latencies_to_file(stamped_latencies, latencies_filename)

if __name__ == '__main__':
    asyncio.run(main())
