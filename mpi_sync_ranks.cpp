#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_map>
#include <vector>
#include <iostream>

typedef long node_id;
typedef double rank;
typedef std::pair<node_id, rank> node_pair;

std::unordered_map<node_id, rank> *curr_ranks;
std::unordered_map<node_id, rank> *next_ranks;
// For every pid, stores the list of local nodes that thread is interested in.
std::vector<std::vector<node_id> > out_facing_nodes_by_pid;
// Number of nodes we expect to receive from this pid
std::vector<size_t> num_incoming_nodes_by_pid;

int mypid;
int numprocs;

void send_ranks()
{
  for (int dest_pid = 0; dest_pid < numprocs; dest_pid++)
  {
    auto &&nodes = out_facing_nodes_by_pid[dest_pid];

    if (nodes.size() == 0)
    {
      continue;
    }

    auto rank_updates = new node_pair[nodes.size()];
    for (size_t i = 0; i < nodes.size(); i++)
    {
      rank_updates[i].first = nodes[i];
      rank_updates[i].second = (*next_ranks)[nodes[i]];
    }

    MPI_Ssend(rank_updates, nodes.size() * sizeof(node_pair), MPI_CHAR, dest_pid, 1, MPI_COMM_WORLD);
  }
}

void recv_ranks_for(int pid)
{
  size_t num_incoming_nodes = num_incoming_nodes_by_pid[pid];
  auto rank_updates = new node_pair[num_incoming_nodes];
  MPI_Status stat;
  MPI_Recv(rank_updates, num_incoming_nodes * sizeof(node_pair), MPI_CHAR, pid, 1, MPI_COMM_WORLD, &stat);
  
  (*next_ranks).insert(rank_updates, rank_updates + num_incoming_nodes);
  
}

void sync_ranks()
{
  for (int curr_pid = 0; curr_pid < numprocs; curr_pid++)
  {
    if (curr_pid == mypid)
    {
      std::cout << "sending thread " << mypid << "\n";
      send_ranks();
    }
    else
    {
      recv_ranks_for(curr_pid);
    }
  }
}

void push_mock_data()
{
  (*next_ranks)[mypid] = 12;
  for (int pid = 0; pid < numprocs; pid++)
  {
    if (pid == mypid)
    {
      num_incoming_nodes_by_pid[pid] = 0;
      continue;
    }
    out_facing_nodes_by_pid[pid].push_back(mypid);
    num_incoming_nodes_by_pid[pid] = 1;
  }
}

void init_vars(){
  num_incoming_nodes_by_pid.reserve(numprocs);
  out_facing_nodes_by_pid.reserve(numprocs);

  for (int pid = 0; pid < numprocs; pid++)
  {
    num_incoming_nodes_by_pid.push_back(0);
    out_facing_nodes_by_pid.push_back(std::vector<node_id>());
  }
}

int main(int argc, char *argv[])
{
  int src;
  int dest;
  char *sdata;
  char *rdata;
  int count;
  MPI_Status stat;

  MPI_Init(&argc, &argv); /* starts MPI */

  MPI_Comm_rank(MPI_COMM_WORLD, &mypid);    /* get current process id */
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs); /* get number of processes */


  curr_ranks = new std::unordered_map<node_id, rank>;
  next_ranks = new std::unordered_map<node_id, rank>;
  init_vars();
  push_mock_data();
  sync_ranks();
  
  for (auto &&item: *next_ranks)
  {
    std::cout << item.first << ": " << item.second << "\n";
  }

  MPI_Finalize();
  return 0;
}
