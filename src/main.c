#include <stdio.h>
#include <math.h>
#include "array.h"
#include "display.h"
#include "vector.h"
#include "mesh.h"
#include "matrix.h"
#include "light.h"

triangle_t* triangles_to_render = NULL;

vec3_t camera_position = { 0, 0, 0 };
mat4_t proj_matrix;

bool is_running = false;
enum cull_method cull_method;
enum render_method render_method;

uint32_t previous_frame_time = 0; 

int depth_comparator(const void *p, const void *q) 
{
    float l = ((triangle_t*) p)->avg_depth;
    float r = ((triangle_t*) q)->avg_depth; 
    
    if (l > r)
        return -1;
    else if (l == r)
        return 0;
    else
        return 1;
}

bool setup(void)
{
    render_method = RENDER_WIRE;
    cull_method = CULL_BACKFACE;

    color_buffer = (uint32_t*) malloc(sizeof(uint32_t) * window_width * window_height);

    color_buffer_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        window_width,
        window_height
    );

    // Initialize the perspective projection matrix
    float fov = M_PI / 3.0; // (180 / 3 -> 60deg)
    float aspect = (float)window_height / (float)window_width;
    float znear = 0.1;
    float zfar = 100.0;
    proj_matrix = mat4_make_perspective(fov, aspect, znear, zfar);

    char* filename ="./assets/f22.obj";
    if (!load_obj_file_data(filename))
    {
        fprintf(stderr, "Error loading mesh obj file %s.", filename);
        return false;
    }

    //load_cube_mesh_data();
    return true;
}

void process_input(void)
{
    SDL_Event event;
    SDL_PollEvent(&event);

    switch (event.type)
    {
    case SDL_QUIT:
        is_running = false;
        break;
    case SDL_KEYDOWN:
        switch (event.key.keysym.sym)
        {
        case SDLK_ESCAPE:
            is_running = false;
            break;
        case SDLK_c:
            cull_method = CULL_BACKFACE;
            break;
        case SDLK_d:
            cull_method = CULL_NONE;
            break;
        case SDLK_1:
            render_method = RENDER_WIRE_VERTEX;
            break;
        case SDLK_2:
            render_method = RENDER_WIRE;
            break;
        case SDLK_3:
            render_method = RENDER_FILL_TRIANGLE;
            break;
        case SDLK_4:
            render_method = RENDER_FILL_TRIANGLE_WIRE;
            break;
        case SDLK_5:
            render_method = RENDER_FLAT_SHADING;
            break;
        case SDLK_6:
            render_method = RENDER_FLAT_SHADING_WIRE;
            break;
        }
        break;
    }
}

void update(void)
{ 
    uint32_t time_to_wait = FRAME_TARGET_TIME - (SDL_GetTicks() - previous_frame_time);

    if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME)
    {
        SDL_Delay(time_to_wait);
    }

    previous_frame_time = SDL_GetTicks();
    
    // Initialize the array of triangles to render
    triangles_to_render = NULL;

    // Change the mesh scale/rotation values per animation frame
    mesh.rotation.x += 0.005;
    // mesh.rotation.y += 0.01;
    // mesh.rotation.z += 0.01;
    // mesh.scale.x += 0.002;
    // mesh.scale.y += 0.001;
    // mesh.translation.x += 0.01;
    mesh.translation.z = 5.0;

    // Scale, rotation and translation matrices
    mat4_t scale_matrix = mat4_make_scale(mesh.scale.x, mesh.scale.y, mesh.scale.z);
    mat4_t rotation_matrix_x = mat4_make_rotation_x(mesh.rotation.x);
    mat4_t rotation_matrix_y = mat4_make_rotation_y(mesh.rotation.y);
    mat4_t rotation_matrix_z = mat4_make_rotation_z(mesh.rotation.z);
    mat4_t translation_matrix = mat4_make_translation(mesh.translation.x, mesh.translation.y, mesh.translation.z);

    // Create the World Matrix, combining scale, rotations and transformation matrices
    // (order matters: scale -> rotate -> translate)
    mat4_t world_matrix = mat4_identity();
    world_matrix = mat4_mul_mat4(scale_matrix, world_matrix);
    world_matrix = mat4_mul_mat4(rotation_matrix_z, world_matrix);
    world_matrix = mat4_mul_mat4(rotation_matrix_y, world_matrix);
    world_matrix = mat4_mul_mat4(rotation_matrix_x, world_matrix);
    world_matrix = mat4_mul_mat4(translation_matrix, world_matrix);

    // Loop all triangle faces for the mesh
    int num_faces = array_length(mesh.faces);
    for (int i = 0; i < num_faces; i++)
    {
        face_t mesh_face = mesh.faces[i];

        vec3_t face_vertices[3];
        face_vertices[0] = mesh.vertices[mesh_face.a - 1];
        face_vertices[1] = mesh.vertices[mesh_face.b - 1];
        face_vertices[2] = mesh.vertices[mesh_face.c - 1];

        vec4_t transformed_vertices[3];

        // Loop all three vertices of the current face and apply transformation 
        for (int j = 0; j < 3; j++)
        {
            vec4_t transformed_vertex = vec4_from_vec3(face_vertices[j]);

            // Multiply the world matrix by the original vector
            transformed_vertex = mat4_mul_vec4(world_matrix, transformed_vertex);
            
            // Save the transformed vertex
            transformed_vertices[j] = transformed_vertex;
        }

        // Calculate the normal
        vec3_t vector_a = vec3_from_vec4(transformed_vertices[0]);
        vec3_t vector_b = vec3_from_vec4(transformed_vertices[1]);
        vec3_t vector_c = vec3_from_vec4(transformed_vertices[2]);
        
        // Calculate AB and AC
        vec3_t vector_ab = vec3_sub(vector_b, vector_a);
        vec3_normalize(&vector_ab);
        vec3_t vector_ac = vec3_sub(vector_c, vector_a);
        vec3_normalize(&vector_ac);

        // Calculate the Normal
        vec3_t normal = vec3_cross(vector_ab, vector_ac);
        vec3_normalize(&normal);

        // Check backface culling
        if (cull_method == CULL_BACKFACE)
        {
            // Find the camera ray vector
            vec3_t camera_ray = vec3_sub(camera_position, vector_a);

            // Calculate the dot product between the normal and the camera ray
            float dot_normal_camera = vec3_dot(normal, camera_ray);

            if (dot_normal_camera < 0)
            {
                continue;
            }
        }

        vec4_t projected_points[3];

        // Project the three vertices
        for (int j = 0; j < 3; j++)
        {
            // Project the current vertex
            projected_points[j] = mat4_mul_vec4_project(proj_matrix, transformed_vertices[j]);

            // Scale int the view
            projected_points[j].x *= (window_width / 2.0);
            projected_points[j].y *= (window_height / 2.0);

            // Invert the y values to account for flipped screen y-coordinate
            projected_points[j].y *= -1;

            // Translate projected points to the middle of the screen
            projected_points[j].x += (window_width / 2.0);
            projected_points[j].y += (window_height / 2.0);
        }

        // Calculate the average depth for each face, based on the vertices after the transformation
        float avg_depth = (transformed_vertices[0].z + transformed_vertices[1].z + transformed_vertices[2].z) / 3.0;

        // Calculate the shade intensity based on how aligned is the face normal and the opposite of the light direction
        float light_intensity_factor = -vec3_dot(normal, light.direction);

        // Calculate the triangle color based on the light angle
        uint32_t triangle_color = (render_method == RENDER_FLAT_SHADING || render_method == RENDER_FLAT_SHADING_WIRE) ? 
                                    light_apply_intensity(0xFFFFFFFF, light_intensity_factor) : 
                                    mesh_face.color;

        triangle_t projected_triangle = {
            .points = {
                { projected_points[0].x, projected_points[0].y },
                { projected_points[1].x, projected_points[1].y },
                { projected_points[2].x, projected_points[2].y },
            },
            .color = (render_method == RENDER_FLAT_SHADING || render_method == RENDER_FLAT_SHADING_WIRE) ? triangle_color : mesh_face.color,
            .avg_depth = avg_depth
        };

        // Save the projected triangle in the array of triangle to render
        array_push(triangles_to_render, projected_triangle);
    }

    // TODO: Sort the triangles to render by their avg_depth
    qsort(triangles_to_render, array_length(triangles_to_render), sizeof(triangle_t), depth_comparator);
}

void render(void)
{
    draw_grid(100);

    // Loop all projected triangles and render them
    int num_triangles = array_length(triangles_to_render);
    for (int i = 0; i < num_triangles ; i++)
    {
        triangle_t triangle = triangles_to_render[i];

        if (render_method == RENDER_FILL_TRIANGLE || render_method == RENDER_FILL_TRIANGLE_WIRE || render_method == RENDER_FLAT_SHADING || render_method == RENDER_FLAT_SHADING_WIRE)
        {
            // Draw the filled triangle
            draw_filled_triangle(
                triangle.points[0].x, triangle.points[0].y,
                triangle.points[1].x, triangle.points[1].y,
                triangle.points[2].x,triangle.points[2].y,
                triangle.color
            );
        }
        
        if (render_method == RENDER_WIRE || render_method == RENDER_WIRE_VERTEX || render_method == RENDER_FILL_TRIANGLE_WIRE || render_method == RENDER_FLAT_SHADING_WIRE)
        {
            // Draw the triangle wireframe
            draw_triangle(
                triangle.points[0].x, triangle.points[0].y,
                triangle.points[1].x, triangle.points[1].y,
                triangle.points[2].x,triangle.points[2].y,
                0xFFFFFFFF
            );
        }

        if (render_method == RENDER_WIRE_VERTEX)
        {
            // Draw the triangle wireframe
            draw_rect(triangle.points[0].x, triangle.points[0].y, 4, 4, 0xFFFF0000);
            draw_rect(triangle.points[1].x, triangle.points[1].y, 4, 4, 0xFFFF0000);
            draw_rect(triangle.points[2].x, triangle.points[2].y, 4, 4, 0xFFFF0000);
        }
    }

    //draw_filled_triangle(300, 100, 50, 400, 500, 700, 0xFF00FF00);

    // Clear the array of triangles to render
    array_free(triangles_to_render);

    render_color_buffer();
    clear_color_buffer(0xFF000000);

    SDL_RenderPresent(renderer);
}

void free_resources(void)
{
    free(color_buffer);
    array_free(mesh.faces);
    array_free(mesh.vertices);
}

int main (void)
{
	is_running = initialize_window();

    if (!setup())
    {
        return -1;
    }

    while (is_running)
    {
        process_input();
        update();
        render();
    }

    destroy_window();
    free_resources();

	return 0;
}