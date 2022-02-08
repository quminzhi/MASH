FROM ubuntu:20.04

ENV LOOP_COUNT 10000
RUN apt update && apt install cmake -y

WORKDIR /root
COPY . .
RUN chmod a+x startup.sh

ENTRYPOINT /bin/bash /root/startup.sh
