#pragma once
#include "../headers.hxx"
#include "../definitions.hxx"
#include "../packet/AnnouncementPacket.hxx"

/**
 * Represent a line in the FIB
 */
struct FIBLine {
    std::string prefix;
    // The face is actually the ip address of next router or destination server.
    std::string face;
    int delay = 0;
    int hops = 0;
    std::string tag;

    // a score is a comprehensive representing of several metrics
    // higher score means higher capability
    double score = std::numeric_limits<double>::min();
    // score when considering dynamic metrics like queue size
    double dynamic_score = std::numeric_limits<double>::min();

    std::string ToString();
};


/**
 * The forwarding information base
 */
struct FIB {
    std::vector<FIBLine*> table;
    
    /**
     * Insert or update the table, based on the announcement packet received.
     * @param packet The received packet.
     */
    void InsertOrUpdate(AnnouncementPacket* packet);

    /**
     * Look up the table to make a routing decision.
     * Based on the configuration, multiple paths may be returned.
     */
    std::vector<FIBLine*> LookUp(std::string request_path);

    /**
     * Compute the score for each route in table.
     */
    void ComputeScore();

    /**
     * Sort the table based on the score.
     * Note: dynamic_score is not considered.
     */
    void SortTable();
};