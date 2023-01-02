from tqdm import tqdm

f = open("graph.txt", "r")

lines = f.readlines()

f.close()

vertex_map = {}

edge_weights = {}

adjacency_map = {}

def convert_to_id(vertice):
    if vertice not in vertex_map.keys():
        vertex_map[vertice] = len(vertex_map) + 1 #vertices are enumareted starting from 1
        adjacency_map[vertex_map[vertice]] = set()
    return vertex_map[vertice]
def get_edge_id(vertice_id1, vertice_id2):
    if vertice_id1 > vertice_id2:
        vertice_id1 , vertice_id2 = vertice_id2, vertice_id1
    assert(vertice_id1 <= vertice_id2)
    return (vertice_id1, vertice_id2)
result = ""
for line in tqdm(lines, desc="Processing edges"):
    
    
    [vertice1 , vertice2] = line.split()

    vertice1, vertice2 = convert_to_id(vertice1), convert_to_id(vertice2)
    result += f"{vertice1}\t{vertice2}\n"
    if vertice1 == vertice2:
        continue

    edge = get_edge_id(vertice1, vertice2)

    if edge not in edge_weights:
        edge_weights[edge] = 1
    else:
        edge_weights[edge] += 1
    
    adjacency_map[vertice1].add(vertice2)
    adjacency_map[vertice2].add(vertice1)
    
f = open("id_edges", "w+")
f.write(result)
f.close()
f = open("METIS_to_graph_map", "w+")
result = ""
for v in tqdm(vertex_map.keys(), desc="writing mapping"):
    result += str(v)  + "\n"
f.write(result)
f.close()


f = open("METIS_graph", "w+")
f.write(f'{len(vertex_map)} {len(edge_weights)} 001\n')
result = ""
for vertice in tqdm(range(1, len(vertex_map) + 1), desc="Creating METIS input file"):

    for v in adjacency_map[vertice]:
        result += (f"{v} {edge_weights[get_edge_id(vertice, v)]} ")
    result += ("\n")

print("Writing to secondary memory.")
f.write(result)
f.close()



