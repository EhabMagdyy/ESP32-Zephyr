
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/net/socket.h>
#include <zephyr/kernel.h>
#include <zephyr/net/icmp.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_if.h>

// The handler MUST return enum net_verdict and match this exact signature
enum net_verdict icmp_echo_reply_handler(
										struct net_icmp_ctx *ctx,
										struct net_pkt *pkt,
										struct net_icmp_ip_hdr *ip_hdr,
										struct net_icmp_hdr *icmp_hdr,
										void *user_data){
    printk("Ping reply received!\n");
    return NET_OK;
}

void ping(char* ipv4_addr, uint8_t count){
    int ret;
    struct net_icmp_ctx icmp_context;
    struct net_if *iface = net_if_get_default();
    struct sockaddr_in dst_addr;

    // 1. Parse the target address
    if (net_addr_pton(AF_INET, ipv4_addr, &dst_addr.sin_addr) < 0) {
        printk("Invalid IPv4 address\n");
        return;
    }
    dst_addr.sin_family = AF_INET;

    /* 2. Initialize context (Register the listener)
     * Arguments: context, family, type, code, handler 
     */
    ret = net_icmp_init_ctx(&icmp_context, 
                           AF_INET, 
                           NET_ICMPV4_ECHO_REPLY, 
                           0, 
                           icmp_echo_reply_handler);
    
    if (ret != 0) {
        printk("Failed to init ICMP context, err: %d\n", ret);
        return;
    }

    // 3. Send the echo requests in a loop 
    for (int i = 0; i < count; i++){
        printk("Sending Ping #%d to %s...\n", i + 1, ipv4_addr);
        
        ret = net_icmp_send_echo_request(&icmp_context,
                                        iface,
                                        (struct sockaddr *)&dst_addr,
                                        NULL, // No extra data
                                        NULL);// No extra user data
        
        if (ret != 0) {
            printk("Failed to send ping, err: %d\n", ret);
        }

        k_sleep(K_SECONDS(2));
    }

    // 4. Cleanup when finished
    net_icmp_cleanup_ctx(&icmp_context);
}