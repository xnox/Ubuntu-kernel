/*
 * Copyright (C) 2011 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>

#define MAX_GRAPHS		(2)
#define AVERAGE_SAMPLES		(1)

#define	OPT_DRAW_TAGS		(0x0000001)

static int opts;
static char *opt_title = "Current Drawn over Time";
static char *opt_output = NULL;
static int average_samples =	AVERAGE_SAMPLES;

/* Single point of data, optionally a tag */
typedef struct point_t {
	struct point_t *next;
	float x;
	float y;
	char *tag;		/* NULL if data, else a tag name */
} point_t;

typedef struct {
	char *filename;		/* Name of source log file */
	char *title;		/* Optional graph title */

	/* Data set min/max */
	float x_min;
	float x_max;
	float y_min;
	float y_max;

	/* Graph bounding box min/max (because of tags) */
	float x_graph_min;
	float x_graph_max;

	point_t *head;		/* Point data list head */
	point_t *tail;		/* Point data list tail */
} graph_data_t;

void graph_append_point(graph_data_t *graph, point_t *p)
{
	if (graph->head == NULL) {
		graph->head = p;
		graph->tail = p;
	} else {
		graph->tail->next = p;
		graph->tail = p;
	}
}

point_t *find_next_point(point_t *p)
{
	p = p->next;

	while (p) {
		if (p->tag == NULL)
			return p;
		p = p->next;
	}
	return NULL;
}

void graph_free(graph_data_t *graph, int n)
{
	int i;

	for (i=0; i<n; i++) {
		point_t *p = graph[i].head;
		if (graph[i].filename)
			free(graph[i].filename);
		if (graph[i].title)
			free(graph[i].title);
		
		while (p) {
			point_t *p_next = p->next;
			if (p->tag)
				free(p->tag);
			free(p);
			p = p_next;
		}
	}
}

int graph_load(char *filename, graph_data_t *graph)
{
	FILE *fp;
	char buffer[4096];
	float start_secs = -1.0;
	float last_y = 0.0;

	graph->filename = strdup(filename);
	graph->head = NULL;
	graph->tail = NULL;

	graph->x_min = 1E6;
	graph->x_max = -1E6;
	graph->y_min = 1E6;
	graph->y_max = -1E6;

	graph->x_graph_min = 1E6;
	graph->x_graph_max = -1E6;

	if ((fp = fopen(filename, "r")) == NULL) 
		return -1;

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		float hrs;
		float min;
		float sec;
		point_t *p;

		buffer[strlen(buffer)-1] = '\0';

		sscanf(buffer + 12, "%f:%f:%f", &hrs, &min, &sec);

		sec = sec + (min * 60.0) + (hrs * 3600.0);
		if (start_secs < 0.0)
			start_secs = sec;

		sec = sec - start_secs;
		if (sec < graph->x_graph_min)
			graph->x_graph_min = sec;
		if (sec > graph->x_graph_max)
			graph->x_graph_max = sec;
		
		if (strstr(buffer+27, "SAMPLE:")) {
			if ((p = calloc(1, sizeof(point_t))) == NULL) {
				fprintf(stderr, "Out of memory allocating a data point\n");
				break;
			}
			p->x = sec;
			sscanf(buffer + 34, "%f", &p->y);
			p->tag = NULL;
			last_y = p->y;

			if (sec < graph->x_min)
				graph->x_min = sec;
			if (sec > graph->x_max)
				graph->x_max = sec;

			if (p->y < graph->y_min)
				graph->y_min = p->y;
			if (p->y > graph->y_max)
				graph->y_max = p->y;

			graph_append_point(graph, p);
		}
		if (strstr(buffer+27, "TAG:")) {
			char *ptr = buffer+32;
			ptr = strstr(ptr, " ");
			if ((p = calloc(1, sizeof(point_t))) == NULL) {
				fprintf(stderr, "Out of memory allocating a tag data point\n");
				break;
			}
			p->x = sec;
			p->y = last_y;
			p->tag = calloc(1, strlen(ptr) + 1);
			strcpy(p->tag, ptr);
			graph_append_point(graph, p);
		}
	}
	fclose(fp);

	return 0;
}

int graph_plot(graph_data_t *graphs, int n)
{
	int i;
	pid_t pid;
	FILE *plot;
	char filename[PATH_MAX];
	char cmd[1024];
	int filename_len_max = 0;

	pid = getpid();

	float x_min = 1E6;
	float x_max = -1E6;
	float y_min = 1E6;
	float y_max = -1E6;
	float h;

	for (i=0; i<n; i++) {
		size_t len = strlen(graphs[i].filename);

		if (graphs[i].x_min < x_min)
			x_min = graphs[i].x_min;
		if (graphs[i].x_max > x_max)
			x_max = graphs[i].x_max;
		if (graphs[i].y_min < y_min)
			y_min = graphs[i].y_min;
		if (graphs[i].y_max > y_max)
			y_max = graphs[i].y_max;
		if (len > filename_len_max)
			filename_len_max = len;
	}

	h = y_max - y_min;

	snprintf(filename, sizeof(filename), "/tmp/plot_%d.gnu", pid);
	if ((plot = fopen(filename, "w")) == NULL) {
		fprintf(stderr, "Cannot create %s gnu plot file.\n", filename);
		return -1;
	}

	fprintf(plot, "set terminal png size 1024,768\n");
	fprintf(plot, "set autoscale\n");
	fprintf(plot, "set xtic auto\n");
	fprintf(plot, "set ytic auto\n");
	fprintf(plot, "set title '%s'\n", opt_title);
	fprintf(plot, "set xlabel 'Time (seconds)'\n");
	fprintf(plot, "set ylabel 'Current (Amps)'\n");
	fprintf(plot, "set xrange [%f:%f]\n", x_min, x_max);

	if (opts & OPT_DRAW_TAGS) {
		for (i=0; i<n; i++) {
			point_t *p = graphs[i].head;
	
			while (p) {
				if (p->tag) {
					float yoffset = y_min + ((h/25)*i);
					fprintf(plot, "set label '%s' at %f,%f\n", p->tag, p->x, yoffset);
					fprintf(plot, "set arrow nohead from %f,%f to %f,%f ls %d lw 0.5\n", p->x, y_min, p->x, y_max, i + 1);
				}
				p = p->next;
			}
		}	
	}

	for (i=0; i<n; i++) {
		char title[1024];
		point_t *p = graphs[i].head;
		float integral = 0.0;

		while (p) {
			if (p->tag == NULL) {
				point_t *p_next = find_next_point(p);
				if (p_next) {
					float dt = p_next->x - p->x;
					float y  = ((p->y + p_next->y) / 2.0);
					integral += (y * dt);
				}
			}
			p = p->next;
		}
		snprintf(title, sizeof(title), "%-*.*s: %8.4fA over %.3f secs, average %.4fmA",
			filename_len_max, filename_len_max,
			graphs[i].filename, 
			integral,
			graphs[i].x_max - graphs[i].x_min,
			1000.0 * integral / (graphs[i].x_max - graphs[i].x_min));
		fprintf(stderr, "%s\n", title);
		graphs[i].title = strdup(title);
	}

	for (i=0; i<n; i++) {
		point_t *p = graphs[i].head;
		FILE *fp;
		float sumy;
		float sumx;
		int j;

		snprintf(filename, sizeof(filename), "/tmp/plot_%s_%d.dat", basename(graphs[i].filename), pid);
		if ((fp = fopen(filename, "w")) == NULL) {
			fprintf(stderr, "Cannot create temporary plot file %s\n", filename);
			return -1;
		}

		fprintf(plot, "%s \"%s\" using 1:2 title \"%s\" with lines%s\n",
			i == 0 ? "plot " : "     ", filename, graphs[i].title, i == n-1 ? "" : ", \\");

		j = 0;
		sumx = 0.0;
		sumy = 0.0;
		while (p) {
			if (p->tag == NULL) {
				sumx += p->x;
				sumy += p->y;
				j++;
				if (j >= average_samples) {
					fprintf(fp, "%f %f\n", sumx / (float)average_samples, sumy / (float)average_samples);
					sumx = 0.0;
					sumy = 0.0;
					j = 0;
				}
			}
			p = p->next;
		}
		if (j) {
			fprintf(fp, "%f %f\n", sumx / j, sumy / j);
		}
		fclose(fp);
	}

	fclose(plot);

	if (opt_output)
		snprintf(cmd, sizeof(cmd), "gnuplot /tmp/plot_%d.gnu > %s", pid, opt_output);
	else		
		snprintf(cmd, sizeof(cmd), "gnuplot /tmp/plot_%d.gnu", pid);
	system(cmd);

	snprintf(filename, sizeof(filename), "/tmp/plot_%d.gnu", pid);
	unlink(filename);
	for (i=0; i<n; i++) {
		snprintf(filename, sizeof(filename), "/tmp/plot_%s_%d.dat", graphs[i].filename, pid);
		unlink(filename);
	}
	return 0;
}

void show_help(char *name)
{
	fprintf(stderr, "Usage: %s [-a average] [-h] [-t] [-T title] [-o output] logfile(s)\n", name);
	fprintf(stderr, "\t-a N, draw average of N points\n");
	fprintf(stderr, "\t-h help\n");
	fprintf(stderr, "\t-t include tag field annotations\n");
	fprintf(stderr, "\t-T graph title\n");
	fprintf(stderr, "\t-o output name of .png graph\n");
	fprintf(stderr, "Note, by default, .png is output to stdout\n");
	
}

int main(int argc, char *argv[])
{	
	int i;
	int opt;
	int n;
	graph_data_t graphs[MAX_GRAPHS];
	graph_data_t *graph = graphs;

	memset(graphs, 0, sizeof(graphs));

	while ((opt = getopt(argc, argv, "a:htT:o:")) != -1) {
		switch (opt) {
		case 'a':
			average_samples = atoi(optarg);
			if (average_samples < 1) {
				fprintf(stderr, "Average over N samples cannot be less than 1\n");
				exit(EXIT_FAILURE);
			}

			break;
		case 't':
			opts |= OPT_DRAW_TAGS;
			break;
		case 'T':
			opt_title = optarg;
			break;
		case 'o':
			opt_output = optarg;
			break;
		case 'h':
			show_help(argv[0]);
			exit(EXIT_SUCCESS);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Expected log filename arguments\n");
		exit(EXIT_FAILURE);
	}

	if (argc - optind > MAX_GRAPHS) {
		fprintf(stderr, "Too many graphs, limited to %d\n", MAX_GRAPHS);
		exit(EXIT_FAILURE);
	}

	n = argc - optind;

	for (i=0; i<n; i++) {
		graph_load(argv[i+optind], graph);
		graph++;
	}

	graph_plot(graphs, n);
	graph_free(graphs, n);

	exit(EXIT_SUCCESS);
}
