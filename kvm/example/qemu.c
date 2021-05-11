#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/kvm.h>
#include <linux/bpf.h>

#define KVM_FILE "/dev/kvm"

int main() {
    struct kvm_sregs sregs;
    struct kvm_regs regs;
    int ret;
    int kvmfd, vmfd;
    int version = 0;

    kvmfd = open(KVM_FILE, O_RDWR);
    version = ioctl(kvmfd, KVM_GET_API_VERSION, NULL);
    printf("KVM_API_VERSION: %d\n", version);

    // Create VM has no virtual cpus and no memory. Using 0 as machine type.
    vmfd = ioctl(kvmfd, KVM_CREATE_VM, 0);
    unsigned char *ram = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED | 
        MAP_ANONYMOUS, -1, 0);
    int kfd = open("test.bin", O_RDONLY);
    read(kfd, ram, 4096);

    struct kvm_userspace_memory_region mem = {
        .slot = 0,                  // 表示不同的内存空间
        .guest_phys_addr = 0,       // 这段空间在虚拟机物理内存空间的位置
        .memory_size = 0x1000,      // 物理空间的大小
        .userspace_addr = (unsigned long)ram,   // 表示这段物理空间对应在宿主机上的虚拟机地址
    };
    ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &mem);

    int vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, 0);
    int mmap_size = ioctl(kvmfd, KVM_GET_VCPU_MMAP_SIZE, NULL);
    // 每一个VCPU都有一个struct kvm_run结构，用来在用户态(本程序)和内核态(KVM)共享数据
    struct kvm_run *run = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED,
        vcpufd, 0);
    ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    sregs.cs.base = 0;
    sregs.cs.selector = 0;

    ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
    
    ret = ioctl(vcpufd, KVM_GET_REGS, &regs);
    regs.rip = 0;
    regs.rflags = 2;
    
    ret = ioctl(vcpufd, KVM_SET_REGS, &regs);


    while(1) {
        ret = ioctl(vcpufd, KVM_RUN, NULL);
        if (ret == -1) {
            printf("exit unknown\n");
            return -1;
        }
        switch(run->exit_reason) {
            case KVM_EXIT_HLT:
                puts("KVM_EXIT_HLT");
                return 0;
            case KVM_EXIT_IO:
                putchar(*(((char *)run) + run->io.data_offset));
                break;
            case KVM_EXIT_FAIL_ENTRY:
                puts("entry error");
                return -1;
            default:
                puts("other error");
                printf("exit_reason: %d\n", run->exit_reason);
            return -1;
        }
    }
}
