/*        S V N _ M O V E D _ A N D _ E D I T E D . C X X
 *
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file svn_moved_and_edited.cxx
 *
 * Searches an SVN dump file for revisions with Node-paths that have both a
 * Note-copyfrom-path property and a non-zero Content-length - the idea being
 * that these are changes were a file was both moved and changed.
 *
 * These are potentially problematic commits when converting to git, since
 * they could break git log --follow
 */

#include "pstream.h"
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <set>
#include <map>

void cvs_init()
{
    std::system("rm -rf brlcad_cvs");
    std::system("tar -xf brlcad_cvs.tar.gz");
    std::system("rm brlcad_cvs/brlcad/src/librt/Attic/parse.c,v");
    std::system("rm brlcad_cvs/brlcad/pix/sphflake.pix,v");
    std::system("cp repaired/sphflake.pix,v brlcad_cvs/brlcad/pix/");
    // RCS headers introduce unnecessary file differences, and file differences
    // can be poison pills for git log --follow
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Date:[^$]*/$Date:/' {} \\;");
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Header:[^$]*/$Header:/' {} \\;");
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Id:[^$]*/$Id:/' {} \\;");
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Log:[^$]*/$Log:/' {} \\;");
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Revision:[^$]*/$Revision:/' {} \\;");
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Source:[^$]*/$Source:/' {} \\;");
    std::system("sed -i 's/$Author:[^$]*/$Author:/' brlcad_cvs/brlcad/misc/Attic/cvs2cl.pl,v");
    std::system("sed -i 's/$Author:[^$]*/$Author:/' brlcad_cvs/brlcad/sh/Attic/cvs2cl.pl,v");
    std::system("sed -i 's/$Locker:[^$]*/$Locker:/' brlcad_cvs/brlcad/src/other/URToolkit/tools/mallocNd.c,v");
}

void cvs_to_git()
{
    std::system("find brlcad_cvs/brlcad/ | cvs-fast-export -A authormap > brlcad_git.fi");
    std::system("rm -rf brlcad_git");
    std::system("mkdir brlcad_git");
    chdir("brlcad_git");
    std::system("git init");
    std::system("cat ../brlcad_git.fi | git fast-import");
    std::system("git checkout master");
    // This repository has as its newest commit a "test acl" commit that doesn't
    // appear in subversion - the newest master commit matching subversion corresponds
    // to r29886.  To clear this commit and label the new head with its corresponding
    // subversion revision, we do:
    std::system("git reset --hard HEAD~1");
    // Done
    chdir("..");
}

int
path_is_branch(std::string path)
{
    if (path.length() <= 15 || path.compare(0, 15, "brlcad/branches")) return 0;
    std::string spath = path.substr(16);
    size_t slashpos = spath.find_first_of('/');
    return (slashpos == std::string::npos) ? 1 : 0;
}

int
path_is_tag(std::string path)
{
    if (path.length() <= 11 || path.compare(0, 11, "brlcad/tags")) return 0;
    std::string spath = path.substr(12);
    size_t slashpos = spath.find_first_of('/');
    return (slashpos == std::string::npos) ? 1 : 0;
}

struct node_info {
    int in_branch = 0;
    int in_tag = 0;
    int have_node_path = 0;
    int is_add = 0;
    int is_delete = 0;
    int is_edit = 0;
    int is_file = 0;
    int is_move = 0;
    int merge_commit = 0;
    int node_path_filtered = 0;
    int move_edit_rev = 0;
    int rev = -1;
    std::string node_path;
    std::string node_copyfrom_path;
};

struct commit_info {
    std::set<int> move_edit_revs;
    std::multimap<int, std::pair<std::string, std::string> > revs_move_map;
    std::set<int> merge_commits;
    std::set<int> tags;
};

void node_info_reset(struct node_info *i)
{
    i->in_branch = 0;
    i->in_tag = 0;
    i->have_node_path = 0;
    i->is_add = 0;
    i->is_delete = 0;
    i->is_edit = 0;
    i->is_file = 0;
    i->is_move = 0;
    i->merge_commit = 0;
    i->node_path_filtered = 0;
    i->move_edit_rev = 0;
    i->rev = -1;
    i->node_path.clear();
    i->node_copyfrom_path.clear();
}

void process_node(struct node_info *i, struct commit_info *c)
{
    if (i->is_move && i->is_edit && i->is_file) {
	std::pair<std::string, std::string> move(i->node_copyfrom_path, i->node_path);
	std::pair<int, std::pair<std::string, std::string> > mvmap(i->rev, move);
	c->revs_move_map.insert(mvmap);
	c->move_edit_revs.insert(i->rev);
    }
    if (i->merge_commit && i->in_branch) {
	if (c->merge_commits.find(i->rev) == c->merge_commits.end()) {
	    c->merge_commits.insert(i->rev);
	}
    }
    if (path_is_branch(i->node_path) && i->is_delete) {
	std::cout << "Branch delete " << i->rev << ": " << i->node_path << "\n";
    }
    if (path_is_branch(i->node_path) && i->is_add) {
	std::cout << "Branch add    " << i->rev << ": " << i->node_path << "\n";
    }
    if (path_is_tag(i->node_path) && i->is_add) {
	std::cout << "Tag add       " << i->rev << ": " << i->node_path << "\n";
	c->tags.insert(i->rev);
    }
    if (path_is_tag(i->node_path) && i->is_delete) {
	std::cout << "Tag delete    " << i->rev << ": " << i->node_path << "\n";
	c->tags.insert(i->rev);
    }
    if (c->tags.find(i->rev) == c->tags.end() && i->in_tag && !(path_is_tag(i->node_path) && i->is_delete)) {
	std::cout << "Tag edit      " << i->rev << ": " << i->node_path << "\n";
    }


}

void characterize_commits(const char *dfile, struct commit_info *c)
{
    int rev = -1;
    struct node_info info;
    std::ifstream infile(dfile);
    std::string line;
    while (std::getline(infile, line))
    {
	std::istringstream ss(line);
	std::string s = ss.str();
	if (!s.compare(0, 16, "Revision-number:")) {
	    if (rev >= 0) {
		// Process last node of previous revision
		process_node(&info, c);
	    }
	    node_info_reset(&info);
	    // Grab new revision number
	    rev = std::stoi(s.substr(17));
	}
	if (rev >= 0) {
	    // OK , now we have a revision - start looking for content
	    if (!s.compare(0, 10, "Node-path:")) {
		if (info.have_node_path) {
		    // Process previous node
		    process_node(&info, c);
		}
		// Have a node - initialize
		node_info_reset(&info);
		info.rev = rev;
		info.have_node_path = 1;
		info.node_path = s.substr(11);
		if (info.node_path.compare(0, 6, "brlcad") != 0) {
		    info.node_path_filtered = 1;
		} else {
		    info.node_path_filtered = 0;
		    if (!info.node_path.compare(0, 15, "brlcad/branches")) {
			info.in_branch = 1;
		    }
		    if (!info.node_path.compare(0, 11, "brlcad/tags")) {
			info.in_tag = 1;
		    }
		}
		//std::cout <<  "Node path: " << node_path << "\n";
	    } else {
		if (info.have_node_path && !info.node_path_filtered) {
		    if (!s.compare(0, 19, "Node-copyfrom-path:")) {
			info.is_move = 1;
			info.node_copyfrom_path = s.substr(19);
			//std::cout <<  "Node copyfrom path: " << node_copyfrom_path << "\n";
		    }
		    if (!s.compare(0, 15, "Content-length:")) {
			info.is_edit = 1;
		    }
		    if (!s.compare(0, 15, "Node-kind: file")) {
			info.is_file = 1;
		    }
		    if (!s.compare(0, 19, "Node-action: delete")) {
			info.is_delete = 1;
		    }
		    if (!s.compare(0, 19, "Node-action: add")) {
			info.is_add = 1;
		    }

		}
		if (!info.merge_commit) {
		    if (!s.compare(0, 13, "svn:mergeinfo")) {
			info.merge_commit = 1;
		    }
		}
	    }
	}
    }
}

std::string
revision_date(const char *repo, int rev)
{
    std::string datestr;
    std::string datecmd = "svn propget --revprop -r " + std::to_string(rev) + " svn:date file://" + std::string(repo);
    redi::ipstream proc(datecmd.c_str(), redi::pstreams::pstdout);
    std::getline(proc.out(), datestr);
    return datestr;
}

std::string
revision_author(const char *repo, int rev)
{
    std::string authorstr;
    std::string authorcmd = "svn propget --revprop -r " + std::to_string(rev) + " svn:author file://" + std::string(repo);
    redi::ipstream proc(authorcmd.c_str(), redi::pstreams::pstdout);
    std::getline(proc.out(), authorstr);
    return authorstr;
}

// Provides both in-memory and in-msg.txt-file versions of log msg, for both
// in memory processing and git add -F msg.txt support.
std::string
revision_log_msg(const char *repo, int rev)
{
    std::string logmsg;
    std::string logcmd = "svn log -r" + std::to_string(rev) + " --xml file://" + std::string(repo) + " > msg.xml";
    std::system(logcmd.c_str());
    std::system("xsltproc svn_logmsg.xsl msg.xml > msg.txt");
    std::ifstream mstream("msg.txt");
    std::stringstream mbuffer;
    mbuffer << mstream.rdbuf();
    logmsg = mbuffer.str();
    return logmsg;
}

void
revision_diff(const char *repo, int rev)
{
    std::string diffcmd = "svn diff -c" + std::to_string(rev) + " > svn.diff";
    std::system(diffcmd.c_str());
    //TODO - characterize branch from diff, checkout (or make) correct branch, format
    //diff appropriately for local application.  Need to detect and handle multi-branch commits
    //somehow, if they occur...
    //
    //TODO - detect and handle merge commits...
}

int
apply_rev(const char *repo, int rev)
{
    int orev = rev - 1;
    revision_diff(repo, rev);
    std::system("patch -f --remove-empty-files -p0 < svn.diff");
    std::system("rm svn.diff");
    std::system("git add -A");
    revision_log_msg(repo, rev);
    std::system("git commit -F msg.txt");
    std::system("rm msg.xml");
    std::system("rm msg.txt");
}

int
apply_split_rev(const char *repo, int rev)
{
    int orev = rev - 1;
    revision_diff(repo, rev);
    std::system("patch -p0 < svn.diff");
    std::system("rm svn.diff");
    std::system("git add -A");
    revision_log_msg(repo, rev);
    std::system("git commit -F msg.txt");
    std::system("rm msg.xml");
    std::system("rm msg.txt");
}



int main(int argc, const char **argv)
{
    int start_svn_rev = 29886;
    struct commit_info c;

    // To obtain a repository copy for processing, do:
    //
    // rsync -av svn.code.sf.net::p/brlcad/code .
    //
    // To produce an efficient dump file for processing, use the
    // incremental and delta options for the subversion dump (we
    // are after the flow of commits and metadata rather than the
    // specific content of the commits, and this produces a much
    // smaller file to parse):
    //
    // svnadmin dump --incremental --deltas /home/user/code

    if (argc != 3) {
	std::cerr << "svn_moved_and_edited <dumpfile> <repository_clone>\n";
	return -1;
    }

    // We begin by processing the CVS repository for commits through r29886
    //cvs_init();
    //cvs_to_git();

    // Commits in SVN that both moved and edited files are problematic for
    // git log --follow, and merge commits to branches need particular handling
    // - find out which ones they are
    characterize_commits(argv[1], &c);
    std::set<int>::iterator iit;
    for (iit = c.merge_commits.begin(); iit != c.merge_commits.end(); iit++) {
	//std::string logmsg = revision_log_msg(argv[2], *iit);
	//std::cout << "Merge commit " << *iit << ": " << logmsg << "\n";
	std::cout << "Merge commit " << *iit << "\n";
    }
    for (iit = c.move_edit_revs.begin(); iit != c.move_edit_revs.end(); iit++) {
	std::multimap<int, std::pair<std::string, std::string> >::iterator rmit;
	std::pair<std::multimap<int, std::pair<std::string, std::string> >::iterator, std::multimap<int, std::pair<std::string, std::string> >::iterator> revrange;
	std::cout << "Move+edit commit: " << *iit << "\n";
	revrange = c.revs_move_map.equal_range(*iit);
	for (rmit = revrange.first; rmit != revrange.second; rmit++) {
	    std::cout << "  " << rmit->second.first << " -> " << rmit->second.second << "\n";
	}
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8