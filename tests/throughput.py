import socket
import time

def throughput_test(server_ip, server_port, num_requests):
    start_time = time.time()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((server_ip, server_port))

        for i in range(num_requests):
            command = f"set key{i} value{i}\n"
            s.sendall(command.encode())
        
            response = s.recv(1024)
            print(response.decode().strip())

    end_time = time.time()
    elapsed_time = end_time - start_time
    print(f"Total time for {num_requests} requests: {elapsed_time:.2f} seconds")
    print(f"Throughput: {num_requests / elapsed_time:.2f} requests/second")

if __name__ == "__main__":
    server_ip = '127.0.0.1'
    server_port = 6381
    num_requests = 1000000 

    throughput_test(server_ip, server_port, num_requests)
