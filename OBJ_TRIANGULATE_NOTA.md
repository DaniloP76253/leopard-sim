# Nota rapida: Triangulate Faces (Blender) para OBJ

Si editas `assets/models/hull.obj` o `assets/models/turret.obj` en Blender, exporta siempre con caras trianguladas.

## Pasos en Blender
1. Abre el modelo y selecciona la malla.
2. Entra a Edit Mode.
3. Presiona A para seleccionar todo.
4. Ve a Mesh > Faces > Triangulate Faces.
5. Exporta como OBJ.

## Verificacion rapida en terminal
Desde la raiz del proyecto:

```bash
awk '/^f /{print NF-1}' assets/models/hull.obj | sort -n | uniq -c
awk '/^f /{print NF-1}' assets/models/turret.obj | sort -n | uniq -c
```

Resultado esperado: solo debe aparecer `3` (todas las caras triangulos).

## Nota
Caras de 4+ vertices (quads/n-gons) pueden provocar crash al cargar con raylib (`LoadModel`).
