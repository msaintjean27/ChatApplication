#include <gtk/gtk.h> //GTK GUI
#include <pthread.h> // POSIX threads for concurrency
#include <sys/socket.h> //sockets
#include <arpa/inet.h> //for inet_addr, htons, etc
#include <string.h> // Strings
#include <unistd.h> //needed for close()
#include <stdlib.h> // for strdup, malloc, free
#include "communication.h" //custom header to define shared constants PORT, MAX_CLIENTS, BUFFER_SIZE

int sock; //Socket file descriptor. Sock holds the network connection socket to the chat server
GtkTextBuffer *text_buffer; //GTK buffer for displaying chat message history
GtkWidget *entry; //GTK Widget for the text entry box

////////////////////////////////////////////////
//appends the text message to the GTK text buffer. Inserts new messages to the display area
void append_text_to_buffer(const char *msg) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(text_buffer, &end); //iterator points to end of the text
    gtk_text_buffer_insert(text_buffer, &end, msg, -1); //inserts message at end
}

///////////////////////////////////////////////
//Wrapper function for safely updating GUI text from other threads.
//This is needed because GTK GUI functions can only be modified from the main GUI thread.
//scheduled with g_idle_add() so GTK updates happen in main loop

gboolean gui_append_text(gpointer data) {
    append_text_to_buffer((char *)data); //appends message to buffer
    free(data); //frees memory for the duplicated string
    return FALSE; //removes callback from GTK idle queue after running
}

/////////////////////////////////////////////////
//Thread function for recieving messages from server

void *receive_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    int len;
//Loop receiving messages as long as there is connection and no errors

    while ((len = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[len] = '\0';
        char *msg = strdup(buffer);
        g_idle_add(gui_append_text, msg);
    }
    return NULL;
}
////////////////////////////////////////////////////
//Function to send a message. This is called with a button or the enter key
void send_message(GtkWidget *widget, gpointer data) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(text) == 0) return;

    send(sock, text, strlen(text), 0);
    send(sock, "\n", 1, 0);

    gtk_entry_set_text(GTK_ENTRY(entry), "");
}

////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv); //initializes GTK library

//Sets up TCP connection to chat server
    struct sockaddr_in server_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0); //create TCP socket

    server_addr.sin_family = AF_INET; //IPv4
    server_addr.sin_port = htons(PORT); //converts port to network byte order
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //connects to localhost

    connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)); //connects to server

//Builds GUI: Creates GTK window and widgets

//main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Chat"); //window titled "Chat"
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300); //default window size 400x300
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL); //connects signal handler to "destroy" for closing window, to stop the GTK main loop/quit app

//VBox
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); //VBox with 5p spacing between children
    gtk_container_add(GTK_CONTAINER(window), vbox); //adds VBox to window

//scrollable window for messages
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);//new scrollable window
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0); //adds scrollbar to VBox


//chat display (read only)
    GtkWidget *text_view = gtk_text_view_new(); //text view widget for a multi-line text display
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE); //read only
    gtk_container_add(GTK_CONTAINER(scroll), text_view); //scrollable
    text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view)); //retrieves text buffer. Buffer holds text content, updated with new messages

//HBox for text input field and send button
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); //creates HBox with 5p spacing for text entry field and send button (side by side)
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0); //packs HBox into VBox. Wont expand vertically.

//text entry widget to type messages
    entry = gtk_entry_new(); //creates entry widget
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0); //packs entry widget into HBox. Expands horisontally.

//Send button
    GtkWidget *send_button = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(hbox), send_button, FALSE, FALSE, 0);

//Connects signals for sending function
    g_signal_connect(send_button, "clicked", G_CALLBACK(send_message), NULL);//Button click signal
    g_signal_connect(entry, "activate", G_CALLBACK(send_message), NULL); //enter key signal

//creates a thread to receive messages from server asynchronously
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_handler, NULL); //starts message listener thread
    pthread_detach(recv_thread); //cleans up thread on exit

//show GUI and start event loop
    gtk_widget_show_all(window); //shows all GTK widgets
    gtk_main(); // GTK main event loop

//cleanup
    close(sock); //closes socket on exit
    return 0;
}
