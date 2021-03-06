/* Requests */
#define OPCODE_REQUEST_WRITE	(comm_opcode_t){.code = 0x00}
#define OPCODE_ALLOW_WRITE	(comm_opcode_t){.code = 0x01}
#define OPCODE_COMMIT_PAGE	(comm_opcode_t){.code = 0x02}
#define OPCODE_LOCK_READ	(comm_opcode_t){.code = 0x03}
#define OPCODE_RESUME_READ	(comm_opcode_t){.code = 0x04}
#define OPCODE_INITIAL_READ (comm_opcode_t){.code = 0x05}
/* Responses */
#define ACKCODE_REQUEST_WRITE	(comm_ackcode_t){.code = 0x05}
#define ACKCODE_ALLOW_WRITE	(comm_ackcode_t){.code = 0x06}
#define ACKCODE_COMMIT_PAGE	(comm_ackcode_t){.code = 0x07}
#define ACKCODE_LOCK_READ	(comm_ackcode_t){.code = 0x08}
#define ACKCODE_RESUME_READ	(comm_ackcode_t){.code = 0x09}
#define ACKCODE_NO_RESPONSE	(comm_ackcode_t){.code = 0x0A}
#define ACKCODE_OP_FAILURE	(comm_ackcode_t){.code = 0x0B}
#define ACKCODE_INITIAL_READ (comm_ackcode_t){.code = 0x0C}

typedef struct {unsigned char code;} comm_opcode_t;
typedef struct {unsigned char code;} comm_ackcode_t;

typedef union {
	comm_opcode_t op;
	comm_ackcode_t ack;
} comm_code_t;

struct comm_message_hdr {

	comm_code_t mcode;

	unsigned long vaddr;
    unsigned long pid;
	unsigned long server_pid;
    unsigned long pgd;    
	int payload_len;

} __attribute__((packed));

struct comm_message_data {

	char payload[0];

} __attribute__((packed));

struct comm_message {

	struct comm_message_hdr hdr;
	struct comm_message_data data;

} __attribute__((packed));
