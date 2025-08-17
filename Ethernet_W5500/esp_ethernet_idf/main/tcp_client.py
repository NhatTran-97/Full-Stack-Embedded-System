import socket

HOST = "192.168.1.123"   # IP của ESP
PORT = 5000

print(f"Connecting to {HOST}:{PORT} ...")
with socket.create_connection((HOST, PORT), timeout=5) as s:
    s.settimeout(5)
    print("Connected. Type text and press Enter (blank line to quit).")

    # đọc banner (nếu có)
    try:
        banner = s.recv(1024)
        if banner:
            print("Server:", banner.decode(errors="ignore").strip())
    except socket.timeout:
        pass

    while True:
        line = input("> ")
        if line == "":
            break
        s.sendall((line + "\n").encode())
        try:
            data = s.recv(4096)
            print("Echo :", data.decode(errors="ignore").rstrip())
        except socket.timeout:
            print("(no data)")


