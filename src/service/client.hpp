#pragma once
namespace magicscan::service {
// Queries service status only; never opens a PhysicalDrive as the client.
// 0: scanner identified; 1: service present but scanner unavailable;
// 2: service not reachable or invalid reply.
int query_status();
}
