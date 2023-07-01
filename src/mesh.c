#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "mesh.h"
#include "array.h"

mesh_t mesh = {
	.vertices = NULL,
	.faces = NULL,
	.rotation = { 0, 0, 0 },
	.scale = { 1.0, 1.0, 1.0 },
	.translation = { 0, 0, 0 }
};

vec3_t cube_vertices[N_CUBE_VERTICES] = {
    { .x = -1, .y = -1, .z = -1 }, // 1
    { .x = -1, .y =  1, .z = -1 }, // 2
    { .x =  1, .y =  1, .z = -1 }, // 3
    { .x =  1, .y = -1, .z = -1 }, // 4
    { .x =  1, .y =  1, .z =  1 }, // 5
    { .x =  1, .y = -1, .z =  1 }, // 6
    { .x = -1, .y =  1, .z =  1 }, // 7
    { .x = -1, .y = -1, .z =  1 }  // 8
};

face_t cube_faces[N_CUBE_FACES] = {
	// front
	{ .a = 1, .b = 2, .c = 3, .color = 0xFFFF0000 },
	{ .a = 1, .b = 3, .c = 4, .color = 0xFFFF0000 },
	// right
	{ .a = 4, .b = 3, .c = 5, .color = 0xFF00FF00 },
	{ .a = 4, .b = 5, .c = 6, .color = 0xFF00FF00 },
	//back
	{ .a = 6, .b = 5, .c = 7, .color = 0xFF0000FF },
	{ .a = 6, .b = 7, .c = 8, .color = 0xFF0000FF },
	// left
	{ .a = 8, .b = 7, .c = 2, .color = 0xFFFFFF00 },
	{ .a = 8, .b = 2, .c = 1, .color = 0xFFFFFF00 },
	// top
	{ .a = 2, .b = 7, .c = 5, .color = 0xFFFF00FF },
	{ .a = 2, .b = 5, .c = 3, .color = 0xFFFF00FF },
	// bottom
	{ .a = 6, .b = 8, .c = 1, .color = 0xFF00FFFF },
	{ .a = 6, .b = 1, .c = 4, .color = 0xFF00FFFF }
};

void load_cube_mesh_data(void)
{
	for (int i = 0; i < N_CUBE_VERTICES; i++)
	{
		vec3_t cube_vertex = cube_vertices[i];
		array_push(mesh.vertices, cube_vertex);
	}

	for (int i = 0; i < N_CUBE_FACES; i++)
	{
		face_t cube_face = cube_faces[i];
		array_push(mesh.faces, cube_face);
	}	
}	

bool load_obj_file_data(char* filename)
{
	FILE* file = NULL;
	char line[256];
	int line_number = 0;

	file = fopen(filename, "r");

	if (!file)
	{
		fprintf(stderr, "Error opening obj file %s.", filename);
		return false;
	}

	while(fgets(line, sizeof(line), file))
	{
		line_number++;
		char* token = strtok(line, " ");
		int num_vertices = 0;
		int num_faces = 0;
		
		// Check if it is a vertex definition
		if (strcmp(token, "v") == 0)
		{
			num_vertices++;
			vec3_t vertex;

			token = strtok(NULL, "\n");
			if (!token)
			{
				fprintf(stderr, "Error reading coordinates for vertex #%d from obj file: %s, line: %d", num_vertices, filename, line_number);
				fclose(file);
				return false;
			}

			int values = sscanf(token, "%f %f %f", &vertex.x, &vertex.y, &vertex.z);
			if (values != 3)
			{
				fprintf(stderr, "Error converting coordinates for vertex #%d from obj file: %s, line: %d", num_vertices, filename, line_number);
				fclose(file);
				return false;
			}

			array_push(mesh.vertices, vertex);
		}
		else if (strcmp(token, "f") == 0) // Check if it is a face definition
		{
			num_faces++;
			int vertex_index[3];
			int texture_index[3];
			int normal_index[3];

			// Expect to have faces with three vertices (triangles)
			for (int i = 0; i < 3; i++)
			{
				token = strtok(NULL, " ");

				if (!token)
				{
					fprintf(stderr, "Error reading vertex #%d for mesh face #%d from obj file: %s, line: %d", i, num_faces, filename, line_number);
					fclose(file);
					return false;
				}

				int values = sscanf(token, "%d/%d/%d", &vertex_index[i], &texture_index[i], &normal_index[i]);

				if (values != 3)
				{
					fprintf(stderr, "Error converting vertex #%d for mesh face #%d from obj file: %s, line: %d", i, num_faces, filename, line_number);
					fclose(file);
					return false;
				}									
			}

			face_t face = { vertex_index[0], vertex_index[1], vertex_index[2], 0xFFFFFFFF };
			array_push(mesh.faces, face);
		}
	}

	fclose(file);
	return true;
}