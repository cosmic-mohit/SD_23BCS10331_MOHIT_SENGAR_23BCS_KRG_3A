# LLD_Implemented — Demo Implementation

This folder contains a minimal demo implementation for the Food Delivery LLD:

- A small single-threaded C++ HTTP backend at `backend/main.cpp` that serves:
  - `GET /v1/restaurants`
  - `GET /v1/restaurants/{id}/menu`
  - `POST /v1/orders` (supports `Idempotency-Key` header)

- A static frontend in `frontend/` (`index.html`, `styles.css`, `app.js`) that calls the backend.

Notes:
- The backend is intentionally minimal and uses plain sockets for portability in a demo setting.
- For best results run it in WSL or a Linux environment with `g++` available.

Build & run (Linux / WSL):

```bash
# from repository root
cd Experiment_10/LLD_Implemented
g++ -std=c++17 backend/main.cpp -O2 -o backend/server
./backend/server
```

Build & run (Windows - MinGW / MSYS2 or Visual Studio):

MinGW (g++):
```powershell
cd Experiment_10\LLD_Implemented
g++ -std=c++17 backend/main.cpp -O2 -o backend\server.exe -lws2_32
.
backend\server.exe
```

Visual Studio (Developer Command Prompt):
```powershell
cl /EHsc backend\main.cpp ws2_32.lib
backend\main.exe
```

Serve the frontend (to avoid file:// origin issues):

```powershell
cd Experiment_10\LLD_Implemented\frontend
python -m http.server 8000
# then open http://localhost:8000 in the browser
```

Notes:
- The backend now supports both POSIX and Windows Winsock. If you prefer not to build C++, you can run the backend in WSL for simplicity.
- Frontend is static and communicates with the backend at `http://localhost:8080`.
