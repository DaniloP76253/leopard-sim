import os

def write_obj(filename, vertices, faces):
    with open(filename, 'w') as f:
        for v in vertices:
            f.write(f"v {v[0]} {v[1]} {v[2]}\n")
        # UVs
        f.write("vt 0.0 0.0\nvt 1.0 0.0\nvt 1.0 1.0\nvt 0.0 1.0\n")
        
        # Triangulate faces
        for face in faces:
            # simple triangle fan from the first vertex
            v0 = face[0]
            for i in range(1, len(face) - 1):
                v1 = face[i]
                v2 = face[i+1]
                f.write(f"f {v0}/1 {v1}/2 {v2}/3\n")

# Hull: 
hull_v = [
    [-1.7, -0.7, -3.0], [1.7, -0.7, -3.0], [1.7, 0.7, -2.5], [-1.7, 0.7, -2.5], # front
    [-1.7, -0.7,  3.0], [1.7, -0.7,  3.0], [1.7, 0.7,  3.0], [-1.7, 0.7,  3.0]  # back
]
hull_f = [
    [1, 2, 3, 4], # front
    [2, 6, 7, 3], # right
    [6, 5, 8, 7], # back
    [5, 1, 4, 8], # left
    [4, 3, 7, 8], # top
    [5, 6, 2, 1]  # bottom
]
write_obj('hull.obj', hull_v, hull_f)

# Turret:
turret_v = [
    [-0.6, -0.45, -1.2], [0.6, -0.45, -1.2], [1.2, -0.45, -0.6], [1.2, -0.45, 1.0], [-1.2, -0.45, 1.0], [-1.2, -0.45, -0.6], # bottom
    [-0.5, 0.45, -1.0], [0.5, 0.45, -1.0], [1.0, 0.45, -0.5], [1.0, 0.45, 1.0], [-1.0, 0.45, 1.0], [-1.0, 0.45, -0.5], # top
]
turret_f = [
    [1, 2, 8, 7], # front flat
    [2, 3, 9, 8], # right cheek
    [3, 4, 10, 9], # right side
    [4, 5, 11, 10], # back
    [5, 6, 12, 11], # left side
    [6, 1, 7, 12], # left cheek
    [7, 8, 9, 10, 11, 12], # top
    [6, 5, 4, 3, 2, 1] # bottom
]
write_obj('turret.obj', turret_v, turret_f)
