import math

def get_normal(p1, p2, p3):
    ux, uy, uz = p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2]
    vx, vy, vz = p3[0] - p1[0], p3[1] - p1[1], p3[2] - p1[2]
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    l = math.sqrt(nx*nx + ny*ny + nz*nz)
    if l == 0: return (0, 1, 0)
    return (nx/l, ny/l, nz/l)

def write_obj(filename, vertices, faces):
    with open(filename, 'w') as f:
        for v in vertices:
            f.write(f"v {v[0]:.3f} {v[1]:.3f} {v[2]:.3f}\n")
        
        uvs = []
        uv_idx = 1
        face_strings = []
        
        for face in faces:
            v0_idx = face[0]
            for i in range(1, len(face) - 1):
                v1_idx = face[i]
                v2_idx = face[i+1]
                
                v0 = vertices[v0_idx]
                v1 = vertices[v1_idx]
                v2 = vertices[v2_idx]
                
                n = get_normal(v0, v1, v2)
                ax, ay, az = abs(n[0]), abs(n[1]), abs(n[2])
                
                scale = 0.25
                face_uvs = []
                for v in [v0, v1, v2]:
                    if ay >= ax and ay >= az:
                        u, v_tex = v[0] * scale, v[2] * scale
                    elif ax >= ay and ax >= az:
                        u, v_tex = v[2] * scale, v[1] * scale
                    else:
                        u, v_tex = v[0] * scale, v[1] * scale
                    
                    uvs.append((u, v_tex))
                    face_uvs.append(uv_idx)
                    uv_idx += 1
                
                face_strings.append(f"f {v0_idx+1}/{face_uvs[0]} {v1_idx+1}/{face_uvs[1]} {v2_idx+1}/{face_uvs[2]}\n")

        for u, v in uvs:
            f.write(f"vt {u:.4f} {v:.4f}\n")
            
        for fs in face_strings:
            f.write(fs)

def make_prism_z(profile_xy, z_min, z_max, v_list, f_list):
    start = len(v_list)
    n = len(profile_xy)
    
    for p in profile_xy:
        v_list.append([p[0], p[1], z_max])
    f_list.append(list(range(start, start + n)))
    
    for p in profile_xy:
        v_list.append([p[0], p[1], z_min])
    f_list.append(list(reversed(range(start + n, start + 2 * n))))
    
    for i in range(n):
        next_i = (i + 1) % n
        f_list.append([start + i, start + n + i, start + n + next_i, start + next_i])

def make_prism_y(profile_xz, y_bottom, y_top, v_list, f_list, slope_x_start=None, slope_y_end=None):
    start = len(v_list)
    n = len(profile_xz)
    
    for p in profile_xz:
        y = y_top
        if slope_x_start is not None and p[0] > slope_x_start:
            y = slope_y_end
        v_list.append([p[0], y, p[1]])
    f_list.append(list(range(start, start + n)))
    
    for p in profile_xz:
        v_list.append([p[0], y_bottom, p[1]])
    f_list.append(list(reversed(range(start + n, start + 2 * n))))
    
    for i in range(n):
        next_i = (i + 1) % n
        f_list.append([start + i, start + n + i, start + n + next_i, start + next_i])

h_v, h_f = [], []
hull_profile = [
    (3.5, -0.7),  
    (3.9,  0.2),  
    (3.0,  0.7),  
    (-3.8,  0.6),  
    (-3.8, -0.3),  
    (-3.6, -0.7),  
]
make_prism_z(hull_profile, -1.6, 1.6, h_v, h_f)

def add_box(v_list, f_list, xmin, xmax, ymin, ymax, zmin, zmax):
    start = len(v_list)
    pts = [
        [xmin, ymin, zmax], [xmax, ymin, zmax], [xmax, ymax, zmax], [xmin, ymax, zmax],
        [xmin, ymin, zmin], [xmax, ymin, zmin], [xmax, ymax, zmin], [xmin, ymax, zmin]
    ]
    for p in pts: v_list.append(p)
    f_list.append([start+0, start+1, start+2, start+3]) # +Z
    f_list.append([start+5, start+4, start+7, start+6]) # -Z
    f_list.append([start+1, start+2, start+6, start+5]) # +X
    f_list.append([start+4, start+0, start+3, start+7]) # -X
    f_list.append([start+3, start+2, start+6, start+7]) # +Y
    f_list.append([start+4, start+5, start+1, start+0]) # -Y

skirt_profile = [
    (3.2, -0.4),
    (3.2,  0.7),
    (-3.5, 0.7),
    (-3.5, -0.4)
]
make_prism_z(skirt_profile, 1.6, 1.75, h_v, h_f)
make_prism_z(skirt_profile, -1.75, -1.6, h_v, h_f)

write_obj('hull.obj', h_v, h_f)

t_v, t_f = [], []

main_body_xz = [
    (1.0, 1.4), 
    (-1.2, 1.4), 
    (-2.5, 1.0), 
    (-2.5, -1.0), 
    (-1.2, -1.4), 
    (1.0, -1.4)
]
make_prism_y(list(reversed(main_body_xz)), 0.0, 0.8, t_v, t_f)

left_cheek_xz = [
    (2.2, 0.5),
    (1.0, 1.4),
    (1.0, 0.5)
]
make_prism_y(list(reversed(left_cheek_xz)), 0.0, 0.8, t_v, t_f, slope_x_start=1.5, slope_y_end=0.4)

right_cheek_xz = [
    (2.2, -0.5),
    (1.0, -0.5),
    (1.0, -1.4)
]
make_prism_y(list(reversed(right_cheek_xz)), 0.0, 0.8, t_v, t_f, slope_x_start=1.5, slope_y_end=0.4)

mantlet_xz = [
    (1.6, 0.5),
    (1.0, 0.5),
    (1.0, -0.5),
    (1.6, -0.5)
]
make_prism_y(list(reversed(mantlet_xz)), 0.2, 0.6, t_v, t_f)

write_obj('turret.obj', t_v, t_f)
