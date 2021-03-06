#include "SeqLib/BamReader.h"


//#define DEBUG_WALKER 1

namespace SeqLib {

// set the bam region
bool _Bam::SetRegion(const GenomicRegion& gp) {

  // mark it "open" again, may be new reads here
  mark_for_closure = false;
    
  //HTS set region 
  if ( (fp->format.format == 4 || fp->format.format == 6) && !idx)  // BAM (4) or CRAM (6)
    idx = std::shared_ptr<hts_idx_t>(sam_index_load(fp.get(), m_in.c_str()), idx_delete());
  
  if (!idx) {
    std::cerr << "Failed to load index for " << m_in << ". Rebuild samtools index" << std::endl;
    return false;
  }
  
  if (gp.chr >= m_hdr.NumSequences()) {
    std::cerr << "Failed to set region on " << gp << ". Chr ID is bigger than n_targets=" << m_hdr.NumSequences() << std::endl;
    return false;
  }
  
  // should work for BAM or CRAM
  hts_itr = std::shared_ptr<hts_itr_t>(sam_itr_queryi(idx.get(), gp.chr, gp.pos1, gp.pos2), hts_itr_delete());
  
  if (!hts_itr) {
    std::cerr << "Error: Failed to set region: " << gp << std::endl; 
    return false;
  }
  
  return true;
}

  bool BamReader::SetPreloadedIndex(const std::string& f, std::shared_ptr<hts_idx_t>& i) {
    if (!m_bams.count(f))
      return false;
    m_bams[f].set_index(i);
    return true;
  }

void BamReader::Reset() {
  for (auto& b : m_bams)
    b.second.reset();
  m_region = GRC();
}

  bool BamReader::Reset(const std::string& f) {
    
    // cant reset what we don't have
    if (!m_bams.count(f))
      return false;
    m_bams[f].reset();
    return true;
}

  bool BamReader::Close() {
    
    bool success = true;
    for (auto& b : m_bams)
      success = success && b.second.close();
    return success;
  }

  bool BamReader::Close(const std::string& f) {
    
    // cant close what we don't have
    if (!m_bams.count(f)) 
      return false;

    return m_bams[f].close();
  }

  bool BamReader::SetPreloadedIndex(SharedIndex& i) {
    if (!m_bams.size())
      return false;
    m_bams.begin()->second.set_index(i);
    return true;
  }
  
  bool BamReader::SetRegion(const GenomicRegion& g) {
    m_region.clear();
    m_region.add(g);
    
    bool success = true;
    if (m_region.size()) {
    for (auto& b : m_bams) {
      b.second.m_region = &m_region;
      b.second.m_region_idx = 0; // set to the begining
      success = success && b.second.SetRegion(m_region[0]);
    }
    return success;
  }

  return false;
  
}

  bool BamReader::SetMultipleRegions(const GRC& grc) 
{
  if (grc.size() == 0) {
    std::cerr << "Warning: Trying to set an empty bam region"  << std::endl;
    return false;
  }
  
  m_region = grc;

  // go through and start all the BAMs at the first region
  bool success = true;
  if (m_region.size()) {
    for (auto& b : m_bams) {
      b.second.m_region = &m_region;
      b.second.m_region_idx = 0; // set to the begining
      success = success && b.second.SetRegion(m_region[0]);
    }
    return success;
  }
  
  return false;
}

  bool BamReader::Open(const std::string& bam) {

    // dont open same bam twice
    if (m_bams.count(bam))
      return false;
    
    _Bam new_bam(bam);
    new_bam.m_region = &m_region;
    bool success = new_bam.open_BAM_for_reading();
    m_bams.insert(std::pair<std::string, _Bam>(bam, new_bam));
    return success;
  }

  bool BamReader::Open(const std::vector<std::string>& bams) {
    
    bool pass = true;
    for (auto& i : bams) // loop and open all of them
      pass = pass && Open(i);
    return pass;
  }
  
BamReader::BamReader() {}

  bool _Bam::open_BAM_for_reading() {
    
    // HTS open the reader
    fp = std::shared_ptr<htsFile>(hts_open(m_in.c_str(), "r"), htsFile_delete()); 
    
    // open cram reference
    if (!m_cram_reference.empty()) {
      char * m_cram_reference_cstr = strdup(m_cram_reference.c_str());
      int ret = cram_load_reference(fp->fp.cram, m_cram_reference_cstr);
      free(m_cram_reference_cstr);
      if (ret < 0) 
	throw std::invalid_argument("Could not read reference genome " + m_cram_reference + " for CRAM opt");
    }
   
    // check if opening failed
    if (!fp) 
      return false; 
    
    // read the header and create a BamHeader
    bam_hdr_t * hdr = sam_hdr_read(fp.get());
    m_hdr = BamHeader(hdr); // calls BamHeader(bam_hdr_t), makes a copy

    // deallocate the memory we just made
    if (hdr)
      bam_hdr_destroy(hdr);
    
    // if BAM header opening failed, return false
    if (!m_hdr.get()) 
      return false;
    
    // everything worked
    return true;
    
  }

  void BamReader::SetCramReference(const std::string& ref) {
    m_cram_reference = ref;
    for (auto& b : m_bams)
      b.second.m_cram_reference = ref;
    
  }

bool BamReader::GetNextRecord(BamRecord& r) {

  // shortcut if we have only a single bam
  if (m_bams.size() == 1) {
    if (m_bams.begin()->second.fp == 0 || m_bams.begin()->second.mark_for_closure) // cant read if not opened
      return false;
    if (m_bams.begin()->second.__load_read(r)) { // try to read
      return true;
    }
    // didn't find anything, clear it
    m_bams.begin()->second.mark_for_closure = true;
    return false;
  }

  // loop the files and load the next read
  // for the one that was emptied last
  for (auto& bam : m_bams) {

    _Bam *tb = &bam.second;

    // if marked, then don't even try on this BAM
    if (tb->mark_for_closure)
      continue;

    // skip un-opened BAMs
    if (tb->fp == 0) 
      continue;

    // if next read is not marked as empty, skip to next
    if (!tb->empty)
      continue; 
    
    // load the next read
    if (tb->__load_read(r)) {
      tb->empty = true;
      tb->mark_for_closure = true; // no more reads in this BAM
      continue; 
    }
    
  }

  // for multiple bams, choose the one to return
  // sort based on chr and left-most alignment pos. Same as samtools
  int min_chr = INT_MAX;
  int min_pos = INT_MAX;
  std::unordered_map<std::string, _Bam>::iterator hit; 
  bool found = false; // did we find a valid read

  for (std::unordered_map<std::string, _Bam>::iterator bam = m_bams.begin(); 
       bam != m_bams.end(); ++bam) {
    
    // dont check if already marked for removal
    if (bam->second.empty || bam->second.mark_for_closure) 
      continue;
    
    found = true;
    if (bam->second.next_read.ChrID() < min_chr || // check if read in this BAM is lowest
	(bam->second.next_read.Position() <  min_pos && bam->second.next_read.ChrID() == min_chr)) {
      min_pos = bam->second.next_read.Position();
      min_chr = bam->second.next_read.ChrID();
      hit = bam; // read is lowest, so mark this BAM as having the hit
    }
  }
  
  // mark the one we just found as empty
  if (found) {
    r = hit->second.next_read; // read is lowest, so assign
    hit->second.empty = true;  // mark as empty, so we fill this slot again
  }
  
  return found;
}
  
std::string BamReader::PrintRegions() const {

  std::stringstream ss;
  for (auto& r : m_region)
    ss << r << std::endl;
  return(ss.str());

}

  bool _Bam::__load_read(BamRecord& r) {

  // allocated the memory
  bam1_t* b = bam_init1(); 
  int32_t valid;

  if (hts_itr == 0) {
    valid = sam_read1(fp.get(), m_hdr.get_(), b);    
    if (valid < 0) { 
      
#ifdef DEBUG_WALKER
      std::cerr << "ended reading on null hts_itr" << std::endl;
#endif
      //goto endloop;
      bam_destroy1(b);
      return false;
    }
  } else {
    
    //changed to sam from hts_itr_next
    // move to next region of bam
    valid = sam_itr_next(fp.get(), hts_itr.get(), b);
  }
  
  if (valid < 0) { // read not found
    do {
      
#ifdef DEBUG_WALKER
      std::cerr << "Failed read, trying next region. Moving counter to " << m_region_idx << " of " << m_region.size() << " FP: "  << fp_htsfile << " hts_itr " << std::endl;
#endif
      // try next region, return if no others to try
      ++m_region_idx; // increment to next region
      if (m_region_idx >= m_region->size()) {
	bam_destroy1(b);
	return false;
      }
	//goto endloop;
      
      // next region exists, try it
      SetRegion(m_region->at(m_region_idx));
      valid = sam_itr_next(fp.get(), hts_itr.get(), b);
    } while (valid <= 0); // keep trying regions until works
  }
  
  // if we got here, then we found a read in this BAM
  empty = false;
  next_read.assign(b); // assign the shared_ptr for the bam1_t
  r = next_read;

  return true;
}

std::ostream& operator<<(std::ostream& out, const BamReader& b)
{
  for (auto& bam : b.m_bams) 
    out << ":" << bam.second.GetFileName() << std::endl; 

  if (b.m_region.size() && b.m_region.size() < 20) {
    out << " ------- BamReader Regions ----------" << std::endl;;
    for (auto& i : b.m_region)
      out << i << std::endl;
  } 
  else if (b.m_region.size() >= 20) {
    int wid = 0;
    for (auto& i : b.m_region)
      wid += i.Width();
    out << " ------- BamReader Regions ----------" << std::endl;;
    out << " -- " << b.m_region.size() << " regions covering " << AddCommas(wid) << " bp of sequence"  << std::endl;
  }
  else 
    out << " - BamReader - Walking whole genome -" << std::endl;

  out <<   " ------------------------------------";
  return out;
}

}
