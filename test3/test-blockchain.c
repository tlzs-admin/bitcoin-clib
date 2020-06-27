/*
 * test-blockchain.c
 * 
 * Copyright 2020 Che Hongwei <htc.chehw@gmail.com>
 * 
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to deal 
 * in the Software without restriction, including without limitation the rights 
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
 * IN THE SOFTWARE.
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "chains.h"
#include <gtk/gtk.h>

typedef struct shell_context
{
	void * user_data;
	void * priv;
	
	int quit;
	
	GtkWidget * window;
	GtkWidget * header_bar;
	GtkWidget * content_area;
	GtkWidget * main_chain;	// the BLOCKCHAIN
	
	ssize_t num_active_chains;
	GtkWidget ** active_chains;

}shell_context_t;

static blockchain_t g_main_chain[1];
static shell_context_t g_shell[1];

shell_context_t * shell_new(int argc, char ** argv, void * user_data);
int shell_init(shell_context_t * shell, void * jconfig);
int shell_run(shell_context_t * shell);
int shell_stop(shell_context_t * shell);
void shell_cleanup(shell_context_t * shell);

int main(int argc, char **argv)
{
	blockchain_t * main_chain = blockchain_init(g_main_chain, NULL, NULL, NULL);
	shell_context_t * shell = shell_new(argc, argv, main_chain);
	assert(shell);

	shell_init(shell, NULL);
	shell_run(shell);
	shell_cleanup(shell);
	
	return 0;
}

enum 
{
	main_chain_column_height,
	main_chain_column_bits,
	main_chain_column_difficulty_accum,
	main_chain_column_timestamp,
	main_chain_column_nonce,
	main_chain_columns_count
};

static void on_cell_data(GtkTreeViewColumn * col, GtkCellRenderer * cr, GtkTreeModel * model, 
	GtkTreeIter * iter, gpointer user_data)
{
	int index = GPOINTER_TO_INT(user_data);
	uint64_t u64 = 0;
	char text[100] = "";
	
	switch(index)
	{
	case main_chain_column_bits:
	case main_chain_column_difficulty_accum:
	case main_chain_column_timestamp:
		gtk_tree_model_get(model, iter, index, &u64, -1);
		snprintf(text, sizeof(text), "0x%.8x", (uint32_t)u64);
		break;
	default:
		abort();
	}
	
	g_object_set(cr, "text", text, NULL);
	
	return;
}

static void init_treeview_main_chain(GtkTreeView * main_chain)
{
	GtkCellRenderer * cr = NULL;
	GtkTreeViewColumn * col = NULL;
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("height", cr, "text", main_chain_column_height, NULL);
	gtk_tree_view_append_column(main_chain, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("bits", cr, "text", main_chain_column_bits, NULL);
	gtk_tree_view_column_set_cell_data_func(col, cr, on_cell_data, 
		GINT_TO_POINTER(main_chain_column_bits), 
		NULL);
	gtk_tree_view_append_column(main_chain, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("difficulty", cr, "text", main_chain_column_difficulty_accum, NULL);
	gtk_tree_view_column_set_cell_data_func(col, cr, on_cell_data, 
		GINT_TO_POINTER(main_chain_column_difficulty_accum),
		NULL);
	gtk_tree_view_append_column(main_chain, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("timestamp", cr, "text", main_chain_column_timestamp, NULL);
	gtk_tree_view_column_set_cell_data_func(col, cr, on_cell_data, 
		GINT_TO_POINTER(main_chain_column_timestamp), 
		NULL);
	gtk_tree_view_append_column(main_chain, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("nonce", cr, "text", main_chain_column_nonce, NULL);
	gtk_tree_view_append_column(main_chain, col);
	
	GtkListStore * store = gtk_list_store_new(main_chain_columns_count, 
		G_TYPE_INT, 
		G_TYPE_UINT, 
		G_TYPE_UINT,
		G_TYPE_INT64, 
		G_TYPE_UINT);
	gtk_tree_view_set_model(main_chain, GTK_TREE_MODEL(store));
	return;
}

shell_context_t * shell_new(int argc, char ** argv, void * user_data)
{
	gtk_init(&argc, &argv);
	shell_context_t * shell = g_shell;
	
	shell->user_data = user_data;
	
	GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget * header_bar = gtk_header_bar_new();
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
	gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "test blockchain ...");
	
	gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);
	gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);
	
	GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_size_request(vbox, 640, 480);	// min size
	gtk_container_add(GTK_CONTAINER(window), vbox);
	
	GtkWidget * frame = gtk_frame_new(NULL);
	GtkWidget * statusbar = gtk_statusbar_new();
	gtk_widget_set_hexpand(statusbar, TRUE);
	
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), statusbar, FALSE, TRUE, 0);
	
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
	
	gtk_widget_set_hexpand(frame, TRUE);
	gtk_widget_set_vexpand(frame, TRUE);
	GtkWidget * grid = gtk_grid_new();
	gtk_container_add(GTK_CONTAINER(frame), grid);
	
	shell->window = window;
	shell->header_bar = header_bar;
	shell->content_area = grid;
	
	GtkWidget * scrolled_win = NULL;
	
	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	GtkWidget * treeview = gtk_tree_view_new();
	gtk_container_add(GTK_CONTAINER(scrolled_win), treeview);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_win), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_widget_set_vexpand(scrolled_win, TRUE);
	gtk_widget_set_hexpand(scrolled_win, TRUE);
	
	gtk_widget_set_size_request(scrolled_win, 300, 180);
	shell->main_chain = treeview;
	
	init_treeview_main_chain(GTK_TREE_VIEW(treeview));
	gtk_widget_set_hexpand(treeview, TRUE);
	gtk_widget_set_vexpand(treeview, TRUE);
	gtk_grid_attach(GTK_GRID(grid), scrolled_win, 0, 0, 1, 1);
	
	g_signal_connect_swapped(window, "destroy", G_CALLBACK(shell_stop), shell);
	return shell;
}

int shell_run(shell_context_t * shell)
{
	gtk_widget_show_all(shell->window);
	gtk_main();
	return 0;
}

int shell_stop(shell_context_t * shell)
{
	if(!shell->quit)
	{
		shell->quit = 1;
		gtk_main_quit();
	}
	return 0;
}

void shell_cleanup(shell_context_t * shell)
{
	shell_stop(shell);
	return;
}


// initialize test dataset

#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include "utils.h"

extern const uint256_t g_genesis_block_hash[1];
extern const struct satoshi_block_header g_genesis_block_hdr[1];

#define MAX_HEIGHT (100)
static uint256_t s_block_hashes[1 + MAX_HEIGHT];
static struct satoshi_block_header s_block_hdrs[1 + MAX_HEIGHT];

static int init_dataset(void)
{
	uint32_t seed = 10000;
	memcpy(&s_block_hashes[0], g_genesis_block_hash, 32);
	
	unsigned char hash[32];
	for(int i = 1; i <= MAX_HEIGHT; ++i)
	{
		struct satoshi_block_header * hdr = &s_block_hdrs[i];
		memcpy(hdr->prev_hash, &s_block_hashes[i - 1], 32);
		hdr->bits = 0x1d00ffff;
		hdr->nonce = seed++;
		hash256(hdr, sizeof(*hdr), (unsigned char *)&s_block_hashes[i]);
	}
	return 0;
}

int shell_init(shell_context_t * shell, void * jconfig)
{
	init_dataset();
	
	blockchain_t * main_chain = shell->user_data;
	assert(main_chain);
	
	for(int i = 1; i <= MAX_HEIGHT; ++i)
	{
		main_chain->add(main_chain, &s_block_hashes[i], &s_block_hdrs[i]);
	}
	
	char text[100];
	snprintf(text, sizeof(text), "blockchain height: %d", (int)main_chain->height);
	gtk_header_bar_set_subtitle(GTK_HEADER_BAR(shell->header_bar), text); 
	
	
	GtkListStore * store = gtk_list_store_new(main_chain_columns_count, 
		G_TYPE_INT, 
		G_TYPE_UINT, 
		G_TYPE_UINT,
		G_TYPE_INT64, 
		G_TYPE_UINT);
	GtkTreeView * treeview = GTK_TREE_VIEW(shell->main_chain);
	assert(treeview);
	GtkTreeIter iter;
	
	const blockchain_heir_t * heirs = main_chain->heirs;
	for(int i = 0; i <= MAX_HEIGHT; ++i)
	{
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 
			main_chain_column_height, i,
			main_chain_column_bits, heirs[i].bits,
			main_chain_column_difficulty_accum, heirs[i].cumulative_difficulty.bits,
			main_chain_column_timestamp, heirs[i].timestamp,
			main_chain_column_nonce, s_block_hdrs[i].nonce,
			-1);
	}
	gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store));
	
	return 0;
}

