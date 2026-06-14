From ubuntu:22.04

Run apt-get update && apt-get install -y g++ make

WORKDIR /app

COPY . .

RUN make

EXPOSE 6739

CMD ["./redis-server"]