#!/bin/bash

MODEL_NAME="${1:-resnet50v2tf}"
TRACE_FILE="${2:-trace}"
DELAY_SCALE="${3:-1}"
NUM_TRACE_READS="${4:-100000}"

INGRESS_HOST=$(kubectl get svc istio-ingressgateway --namespace istio-system -o jsonpath="{.status.loadBalancer.ingress[0].ip}")
INGRESS_PORT=80
SERVICE_HOSTNAME=$(kubectl get inferenceservice ${MODEL_NAME} -o jsonpath='{.status.url}' | cut -d "/" -f 3)

python3 inf.py --ingress_host ${INGRESS_HOST} --service_host ${SERVICE_HOSTNAME} --model_name ${MODEL_NAME} --trace_file ${TRACE_FILE} --delay_scale ${DELAY_SCALE} --num_trace_reads ${NUM_TRACE_READS}