// memory manager. shares pages across many basic blocks.
#include <vector>

using std::vector;


// Provides blocks
class MM {
public:
    MM() {
        PAGE_SIZE_ = getpagesize();
    }

    // alloc into a page aligned buffer or fail.
    void alloc(void** output, int bytes) {
        if (page_capacity_ < bytes) {
            new_page(bytes);
        }
        (*output) = (void*)page_offset_;
        page_offset_ += bytes;
        page_capacity_ -= bytes;
            
    }
private:
    void new_page(int requested) {
        int bytes = requested < PAGE_SIZE_ ? PAGE_SIZE_ : requested;
        if (posix_memalign((void**)&page_, PAGE_SIZE_, bytes) != 0) {
            puts("memalign failed");
            exit(1);
        }
        if (page_ < 0) {
            perror("memalign fail: ");
            exit(1);
        }
        // NOTE: not w^x friendly, pages are both
        // writable and executable.
        if (mprotect((void*)page_, bytes, PROT_EXEC | PROT_READ | PROT_WRITE) < 0) {
            perror("failed to mprotect");
            exit(1);
        }
        page_capacity_ = bytes;
        page_offset_ = page_;
    }

    int PAGE_SIZE_;
    char* page_ = nullptr;
    char* page_offset_ = nullptr;
    int page_capacity_ = 0;

};
