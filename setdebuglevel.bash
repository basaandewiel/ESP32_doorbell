#!/usr/bin/env bash
curl -X GET  "192.168.1.164:80/control?var=tag&val=***"
curl -X GET  "192.168.1.164:80/control?var=loglevel&val=0"

curl -X GET  "192.168.1.164:80/control?var=tag&val=httpd_sess"
curl -X GET  "192.168.1.164:80/control?var=loglevel&val=0"

curl -X GET  "192.168.1.164:80/control?var=tag&val=httpd"
curl -X GET  "192.168.1.164:80/control?var=loglevel&val=0"

curl -X GET  "192.168.1.164:80/control?var=tag&val=httpd_uri"
curl -X GET  "192.168.1.164:80/control?var=loglevel&val=0"

curl -X GET  "192.168.1.164:80/control?var=tag&val=httpd_parse"
curl -X GET  "192.168.1.164:80/control?var=loglevel&val=0"

curl -X GET  "192.168.1.164:80/control?var=tag&val=httpd_txrx"
curl -X GET  "192.168.1.164:80/control?var=loglevel&val=0"

curl -X GET  "192.168.1.164:80/control?var=tag&val=***"
curl -X GET  "192.168.1.164:80/control?var=loglevel&val=0"

curl -X GET  "192.168.1.164:80/control?var=tag&val=***"
curl -X GET  "192.168.1.164:80/control?var=loglevel&val=0"

curl -X GET  "192.168.1.164:80/control?var=tag&val=***"
curl -X GET  "192.168.1.164:80/control?var=loglevel&val=0"

curl -X GET  "192.168.1.164:80/control?var=tag&val=***"
curl -X GET  "192.168.1.164:80/control?var=loglevel&val=0"

