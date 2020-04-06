#include <stdio.h>
#include <assert.h>
#include <msgpack.h>

typedef struct some_struct {
    uint32_t a;
    uint32_t b;
    float c;
} some_struct;

static char *pack(const some_struct *s, int num, int *size);
static some_struct *unpack(const void *ptr, int size, int *num);

/* Fixtures */
some_struct ary[] = {
    { 1234, 5678, 3.14f },
    { 4321, 8765, 4.13f },
    { 2143, 6587, 1.34f }
};

int main(void) {
    /** PACK */
    int size;
    char *buf = pack(ary, sizeof(ary)/sizeof(ary[0]), &size);
    printf("pack %zd struct(s): %d byte(s)\n", sizeof(ary)/sizeof(ary[0]), size);

    /** UNPACK */
    int num;
    some_struct *s = unpack(buf, size, &num);
    printf("unpack: %d struct(s)\n", num);

    /** CHECK */
    assert(num == (int) sizeof(ary)/sizeof(ary[0]));
    for (int i = 0; i < num; i++) {
        assert(s[i].a == ary[i].a);
        assert(s[i].b == ary[i].b);
        assert(s[i].c == ary[i].c);
    }
    printf("check ok. Exiting...\n");

    free(buf);
    free(s);

    return 0;
}

static char *pack(const some_struct *s, int num, int *size) {
    assert(num > 0);
    char *buf = NULL;
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pck;
    msgpack_packer_init(&pck, &sbuf, msgpack_sbuffer_write);
    /* The array will store `num` contiguous blocks made of a, b, c attributes */
    msgpack_pack_array(&pck, 3 * num);
    for (int i = 0; i < num; ++i) {
        msgpack_pack_uint32(&pck, s[i].a);
        msgpack_pack_uint32(&pck, s[i].b);
        msgpack_pack_float(&pck, s[i].c);
    }
    *size = sbuf.size;
    buf = malloc(sbuf.size);
    memcpy(buf, sbuf.data, sbuf.size);
    msgpack_sbuffer_destroy(&sbuf);
    return buf;
}

static some_struct *unpack(const void *ptr, int size, int *num) {
    some_struct *s = NULL;
    msgpack_unpacked msg;
    msgpack_unpacked_init(&msg);
    if (msgpack_unpack_next(&msg, ptr, size, NULL)) {
        msgpack_object root = msg.data;
        if (root.type == MSGPACK_OBJECT_ARRAY) {
            assert(root.via.array.size % 3 == 0);
            *num = root.via.array.size / 3;
            s = malloc(root.via.array.size*sizeof(*s));
            for (int i = 0, j = 0; i < root.via.array.size; i += 3, j++) {
                s[j].a = root.via.array.ptr[i].via.u64;
                s[j].b = root.via.array.ptr[i + 1].via.u64;
                s[j].c = root.via.array.ptr[i + 2].via.f64;
            }
        }
    }
    msgpack_unpacked_destroy(&msg);
    return s;
}
