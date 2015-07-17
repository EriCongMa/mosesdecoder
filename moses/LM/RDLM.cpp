#include "RDLM.h"
#include <vector>
#include "moses/StaticData.h"
#include "moses/ScoreComponentCollection.h"
#include "moses/ChartHypothesis.h"
#include "moses/InputFileStream.h"
#include "moses/Util.h"
#include "util/exception.hh"
#include "neuralTM.h"

namespace Moses
{

typedef Eigen::Map<Eigen::Matrix<int,Eigen::Dynamic,1> > EigenMap;

RDLM::~RDLM()
{
  delete lm_head_base_instance_;
  delete lm_label_base_instance_;
}

void RDLM::Load()
{

  lm_head_base_instance_ = new nplm::neuralTM();
  lm_head_base_instance_->read(m_path_head_lm);

  m_sharedVocab = lm_head_base_instance_->get_input_vocabulary().words() == lm_head_base_instance_->get_output_vocabulary().words();
//   std::cerr << "Does head RDLM share vocabulary for input/output? " << m_sharedVocab << std::endl;

  lm_label_base_instance_ = new nplm::neuralTM();
  lm_label_base_instance_->read(m_path_label_lm);

  if (m_premultiply) {
    lm_head_base_instance_->premultiply();
    lm_label_base_instance_->premultiply();
  }

  lm_head_base_instance_->set_cache(m_cacheSize);
  lm_label_base_instance_->set_cache(m_cacheSize);

  StaticData &staticData = StaticData::InstanceNonConst();
  if (staticData.GetTreeStructure() == NULL) {
    staticData.SetTreeStructure(this);
  }

  offset_up_head = 2*m_context_left + 2*m_context_right;
  offset_up_label = 2*m_context_left + 2*m_context_right + m_context_up;

  size_head = 2*m_context_left + 2*m_context_right + 2*m_context_up + 2;
  size_label = 2*m_context_left + 2*m_context_right + 2*m_context_up + 1;

  UTIL_THROW_IF2(size_head != lm_head_base_instance_->get_order(),
                 "Error: order of head LM (" << lm_head_base_instance_->get_order() << ") does not match context size specified (left_context=" << m_context_left << " , right_context=" << m_context_right << " , up_context=" << m_context_up << " for a total order of " << size_head);
  UTIL_THROW_IF2(size_label != lm_label_base_instance_->get_order(),
                 "Error: order of label LM (" << lm_label_base_instance_->get_order() << ") does not match context size specified (left_context=" << m_context_left << " , right_context=" << m_context_right << " , up_context=" << m_context_up << " for a total order of " << size_label);

  //get int value of commonly used tokens
  static_head_null.resize(size_head);
  for (unsigned int i = 0; i < size_head; i++) {
    char numstr[20];
    sprintf(numstr, "<null_%d>", i);
    static_head_null[i] = lm_head_base_instance_->lookup_input_word(numstr);
  }

  static_label_null.resize(size_label);
  for (unsigned int i = 0; i < size_label; i++) {
    char numstr[20];
    sprintf(numstr, "<null_%d>", i);
    static_label_null[i] = lm_label_base_instance_->lookup_input_word(numstr);
  }

  static_dummy_head = lm_head_base_instance_->lookup_input_word(dummy_head);

  static_start_head = lm_head_base_instance_->lookup_input_word("<start_head>");
  static_start_label = lm_head_base_instance_->lookup_input_word("<start_label>");

  static_head_head = lm_head_base_instance_->lookup_input_word("<head_head>");
  static_head_label = lm_head_base_instance_->lookup_input_word("<head_label>");
  static_head_label_output = lm_label_base_instance_->lookup_output_word("<head_label>");

  static_stop_head = lm_head_base_instance_->lookup_input_word("<stop_head>");
  static_stop_label = lm_head_base_instance_->lookup_input_word("<stop_label>");
  static_stop_label_output = lm_label_base_instance_->lookup_output_word("<stop_label>");
  static_start_label_output = lm_label_base_instance_->lookup_output_word("<start_label>");

  static_root_head = lm_head_base_instance_->lookup_input_word("<root_head>");
  static_root_label = lm_head_base_instance_->lookup_input_word("<root_label>");

  // just score provided file, then exit.
  if (!m_debugPath.empty()) {
    ScoreFile(m_debugPath);
    exit(1);
  }

//   {
//    TreePointer mytree (new InternalTree("[vroot [subj [PPER ich]] [VAFIN bin] [pred [det [ART die]] [attr [adv [adv [PTKNEG nicht]] [ADV fast]] [ADJA neue]] [attr [ADJA europäische]] [NN Zeit]]]"));
//    TreePointer mytree3 (new InternalTree("[ADJA europäische]"));
//    TreePointer mytree4 (new InternalTree("[pred [det [ART die]] [attr [adv [adv [PTKNEG nicht]] [ADV fast]] [ADJA neue]] [attr [ADJA]] [NN Zeit]]]"));
//    TreePointer mytree2 (new InternalTree("[vroot [subj [PPER ich]] [VAFIN bin] [pred]]"));
//
//    std::vector<int> ancestor_heads;
//    std::vector<int> ancestor_labels;
//
//    size_t boundary_hash(0);
//    boost::array<float, 4> score;
//    score.fill(0);
//    std::cerr << "scoring: " << mytree3->GetString() << std::endl;
//    std::vector<TreePointer> previous_trees;
//    TreePointerMap back_pointers = AssociateLeafNTs(mytree3.get(), previous_trees);
//    Score(mytree3.get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash);
//    std::cerr << "head LM: " << score[0] << "label LM: " << score[2] << " approx: " << score[1] << " - " << score[3] << std::endl;
//
//    previous_trees.push_back(mytree3);
//    back_pointers = AssociateLeafNTs(mytree4.get(), previous_trees);
//    std::cerr << "scoring: " << mytree4->GetString() << std::endl;
//    Score(mytree4.get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash);
//    std::cerr << "head LM: " << score[0] << "label LM: " << score[2] << " approx: " << score[1] << " - " << score[3] << std::endl;
//
//    mytree4->Combine(previous_trees);
//    previous_trees.clear();
//    previous_trees.push_back(mytree4);
//    back_pointers = AssociateLeafNTs(mytree2.get(), previous_trees);
//    std::cerr << "scoring: " << mytree2->GetString() << std::endl;
//
//    score[1] = 0;
//    score[3] = 0;
//    Score(mytree2.get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash);
//    std::cerr << "head LM: " << score[0] << "label LM: " << score[2] << " approx: " << score[1] << " - " << score[3] << std::endl;
//
//    score[0] = 0;
//    score[1] = 0;
//    score[2] = 0;
//    score[3] = 0;
//    std::cerr << "scoring: " << mytree->GetString() << std::endl;
//
//    Score(mytree.get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash);
//    std::cerr << "head LM: " << score[0] << "label LM: " << score[2] << " approx: " << score[1] << " - " << score[3] << std::endl;
//
//   }
//    UTIL_THROW2("Finished");
//
//   }
//
//   {
//    std::cerr << "BINARIZED\n\n";
//    TreePointer mytree (new InternalTree("[vroot [subj [PPER ich]] [^vroot [VAFIN bin] [pred [det [ART die]] [^pred [attr [adv [adv [PTKNEG nicht]] [ADV fast]] [ADJA neue]] [^pred [attr [ADJA europäische]] [NN Zeit]]]]]]"));
//    TreePointer mytree3 (new InternalTree("[ADJA europäische]"));
//    TreePointer mytree4 (new InternalTree("[^pred [attr [adv [adv [PTKNEG nicht]] [ADV fast]] [ADJA neue]] [^pred [attr [ADJA]] [NN Zeit]]]"));
//    TreePointer mytree2 (new InternalTree("[vroot [subj [PPER ich]] [^vroot [VAFIN bin] [pred [det [ART die]] [^pred]]]]"));
//
//    std::vector<int> ancestor_heads;
//    std::vector<int> ancestor_labels;
//
//    size_t boundary_hash(0);
//    boost::array<float, 4> score;
//    score.fill(0);
//    std::cerr << "scoring: " << mytree3->GetString() << std::endl;
//    std::vector<TreePointer> previous_trees;
//    TreePointerMap back_pointers = AssociateLeafNTs(mytree3.get(), previous_trees);
//    Score(mytree3.get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash);
//    std::cerr << "head LM: " << score[0] << " label LM: " << score[2] << " approx: " << score[1] << " - " << score[3] << std::endl;
//
//    previous_trees.push_back(mytree3);
//    back_pointers = AssociateLeafNTs(mytree4.get(), previous_trees);
//    std::cerr << "scoring: " << mytree4->GetString() << std::endl;
//    Score(mytree4.get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash);
//    std::cerr << "head LM: " << score[0] << " label LM: " << score[2] << " approx: " << score[1] << " - " << score[3] << std::endl;
//
//    mytree4->Combine(previous_trees);
//    previous_trees.clear();
//    previous_trees.push_back(mytree4);
//    back_pointers = AssociateLeafNTs(mytree2.get(), previous_trees);
//    std::cerr << "scoring: " << mytree2->GetString() << std::endl;
//
//    score[1] = 0;
//    score[3] = 0;
//    Score(mytree2.get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash);
//    std::cerr << "head LM: " << score[0] << " label LM: " << score[2] << " approx: " << score[1] << " - " << score[3] << std::endl;
//
//    score[0] = 0;
//    score[1] = 0;
//    score[2] = 0;
//    score[3] = 0;
//    std::cerr << "scoring: " << mytree->GetString() << std::endl;
//
//    Score(mytree.get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash);
//    std::cerr << "head LM: " << score[0] << " label LM: " << score[2] << " approx: " << score[1] << " - " << score[3] << std::endl;
//
//   }
//    UTIL_THROW2("Finished");

}


void RDLM::Score(InternalTree* root, const TreePointerMap & back_pointers, boost::array<float, 4> &score, std::vector<int> &ancestor_heads, std::vector<int> &ancestor_labels, size_t &boundary_hash, int num_virtual, int rescoring_levels) const
{

  // ignore terminal nodes
  if (root->IsTerminal()) {
    return;
  }

  // ignore glue rules
  if (root->GetLabel() == m_glueSymbol) {
    // recursion
    for (std::vector<TreePointer>::const_iterator it = root->GetChildren().begin(); it != root->GetChildren().end(); ++it) {
      Score(it->get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash, num_virtual, rescoring_levels);
    }
    return;
  }

  // ignore virtual nodes (in binarization; except if it's the root)
  if (m_binarized && root->GetLabel()[0] == '^' && !ancestor_heads.empty()) {
    // recursion
    if (root->IsLeafNT() && m_context_up > 1 && ancestor_heads.size()) {
      root = back_pointers.find(root)->second.get();
      rescoring_levels = m_context_up-1;
    }
    for (std::vector<TreePointer>::const_iterator it = root->GetChildren().begin(); it != root->GetChildren().end(); ++it) {
      Score(it->get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash, num_virtual, rescoring_levels);
    }
    return;
  }

  // ignore start/end of sentence tags
  if (root->GetLabel() == m_startSymbol || root->GetLabel() == m_endSymbol) {
    return;
  }

  nplm::neuralTM *lm_head = lm_head_backend_.get();
  if (!lm_head) {
    lm_head = new nplm::neuralTM(*lm_head_base_instance_);
    lm_head->set_normalization(m_normalizeHeadLM);
    lm_head->set_cache(m_cacheSize);
    lm_head_backend_.reset(lm_head);
  }

  // ignore preterminal node (except if we're scoring root nodes)
  if (root->GetLength() == 1 && root->GetChildren()[0]->IsTerminal()) {
    // root of tree: score without context
    if (ancestor_heads.empty() || (ancestor_heads.size() == m_context_up && ancestor_heads.back() == static_root_head)) {
      std::vector<int> ngram_head_null (static_head_null);
      ngram_head_null.back() = lm_head->lookup_output_word(root->GetChildren()[0]->GetLabel());
      if (m_isPretermBackoff && ngram_head_null.back() == 0) {
        ngram_head_null.back() = lm_head->lookup_output_word(root->GetLabel());
      }
      if (ancestor_heads.size() == m_context_up && ancestor_heads.back() == static_root_head) {
        std::vector<int>::iterator it = ngram_head_null.begin();
        std::fill_n(it, m_context_left, static_start_head);
        it += m_context_left;
        std::fill_n(it, m_context_left, static_start_label);
        it += m_context_left;
        std::fill_n(it, m_context_right, static_stop_head);
        it += m_context_right;
        std::fill_n(it, m_context_right, static_stop_label);
        it += m_context_right;
        size_t context_up_nonempty = std::min(m_context_up, ancestor_heads.size());
        it = std::copy(ancestor_heads.end()-context_up_nonempty, ancestor_heads.end(), it);
        it = std::copy(ancestor_labels.end()-context_up_nonempty, ancestor_labels.end(), it);
      }
      if (ancestor_labels.size() >= m_context_up && !num_virtual) {
        score[0] += FloorScore(lm_head->lookup_ngram(EigenMap(ngram_head_null.data(), ngram_head_null.size())));
      } else {
        boost::hash_combine(boundary_hash, ngram_head_null.back());
        score[1] += FloorScore(lm_head->lookup_ngram(EigenMap(ngram_head_null.data(), ngram_head_null.size())));
      }
    }
    return;
    // we only need to re-visit previous hypotheses if we have more context available.
  } else if (root->IsLeafNT()) {
    if (m_context_up > 1 && ancestor_heads.size()) {
      root = back_pointers.find(root)->second.get();
      // ignore preterminal node
      if (root->GetLength() == 1 && root->GetChildren()[0]->IsTerminal()) {
        return;
      }
      rescoring_levels = m_context_up-1;
    } else {
      return;
    }
  }

  nplm::neuralTM *lm_label = lm_label_backend_.get();
  if (!lm_label) {
    lm_label = new nplm::neuralTM(*lm_label_base_instance_);
    lm_label->set_normalization(m_normalizeLabelLM);
    lm_label->set_cache(m_cacheSize);
    lm_label_backend_.reset(lm_label);
  }

  std::pair<int,int> head_ids;
  bool found = GetHead(root, back_pointers, head_ids);
  if (!found) {
    head_ids = std::make_pair(static_dummy_head, static_dummy_head);
  }

  size_t context_up_nonempty = std::min(m_context_up, ancestor_heads.size());
  const std::string & head_label = root->GetLabel();
  bool virtual_head = false;
  int reached_end = 0;
  int label_idx, label_idx_out;
  if (m_binarized && head_label[0] == '^') {
    virtual_head = true;
    if (m_binarized == 1 || (m_binarized == 3 && head_label[2] == 'l')) {
      reached_end = 1; //indicate that we've seen the first symbol of the RHS
    } else if (m_binarized == 2 || (m_binarized == 3 && head_label[2] == 'r')) {
      reached_end = 2; // indicate that we've seen the last symbol of the RHS
    }
    // with 'full' binarization, direction is encoded in 2nd char
    std::string clipped_label = (m_binarized == 3) ? head_label.substr(2,head_label.size()-2) : head_label.substr(1,head_label.size()-1);
    label_idx = lm_label->lookup_input_word(clipped_label);
    label_idx_out = lm_label->lookup_output_word(clipped_label);
  } else {
    reached_end = 3; // indicate that we've seen first and last symbol of the RHS
    label_idx = lm_label->lookup_input_word(head_label);
    label_idx_out = lm_label->lookup_output_word(head_label);
  }

  int head_idx = (virtual_head && head_ids.first == static_dummy_head) ? static_label_null[offset_up_head+m_context_up-1] : head_ids.first;

  // root of tree: score without context
  if (ancestor_heads.empty() || (ancestor_heads.size() == m_context_up && ancestor_heads.back() == static_root_head)) {
    if (head_idx != static_dummy_head && head_idx != static_head_head) {
      std::vector<int> ngram_head_null (static_head_null);
      *(ngram_head_null.end()-2) = label_idx;
      ngram_head_null.back() = head_ids.second;
      if (ancestor_heads.size() == m_context_up && ancestor_heads.back() == static_root_head && !num_virtual) {
        std::vector<int>::iterator it = ngram_head_null.begin();
        std::fill_n(it, m_context_left, static_start_head);
        it += m_context_left;
        std::fill_n(it, m_context_left, static_start_label);
        it += m_context_left;
        std::fill_n(it, m_context_right, static_stop_head);
        it += m_context_right;
        std::fill_n(it, m_context_right, static_stop_label);
        it += m_context_right;
        it = std::copy(ancestor_heads.end()-context_up_nonempty, ancestor_heads.end(), it);
        it = std::copy(ancestor_labels.end()-context_up_nonempty, ancestor_labels.end(), it);
        score[0] += FloorScore(lm_head->lookup_ngram(EigenMap(ngram_head_null.data(), ngram_head_null.size())));
      } else {
        boost::hash_combine(boundary_hash, ngram_head_null.back());
        score[1] += FloorScore(lm_head->lookup_ngram(EigenMap(ngram_head_null.data(), ngram_head_null.size())));
      }
    }
    std::vector<int> ngram_label_null (static_label_null);
    ngram_label_null.back() = label_idx_out;
    if (ancestor_heads.size() == m_context_up && ancestor_heads.back() == static_root_head && !num_virtual) {
      std::vector<int>::iterator it = ngram_label_null.begin();
      std::fill_n(it, m_context_left, static_start_head);
      it += m_context_left;
      std::fill_n(it, m_context_left, static_start_label);
      it += m_context_left;
      std::fill_n(it, m_context_right, static_stop_head);
      it += m_context_right;
      std::fill_n(it, m_context_right, static_stop_label);
      it += m_context_right;
      it = std::copy(ancestor_heads.end()-context_up_nonempty, ancestor_heads.end(), it);
      it = std::copy(ancestor_labels.end()-context_up_nonempty, ancestor_labels.end(), it);
      score[2] += FloorScore(lm_label->lookup_ngram(EigenMap(ngram_label_null.data(), ngram_label_null.size())));
    } else {
      boost::hash_combine(boundary_hash, ngram_label_null.back());
      score[3] += FloorScore(lm_label->lookup_ngram(EigenMap(ngram_label_null.data(), ngram_label_null.size())));
    }
  }

  ancestor_heads.push_back(head_idx);
  ancestor_labels.push_back(label_idx);

  if (virtual_head) {
    num_virtual = m_context_up;
  } else if (num_virtual) {
    --num_virtual;
  }


  // fill ancestor context (same for all children)
  if (context_up_nonempty < m_context_up) {
    ++context_up_nonempty;
  }
  size_t up_padding = m_context_up - context_up_nonempty;

  std::vector<int> ngram (static_label_null);

  std::vector<int>::iterator it = ngram.begin() + offset_up_head;
  if (up_padding > 0) {
    it += up_padding;
  }

  it = std::copy(ancestor_heads.end() - context_up_nonempty, ancestor_heads.end(), it);

  if (up_padding > 0) {
    it += up_padding;
  }

  it = std::copy(ancestor_labels.end() - context_up_nonempty, ancestor_labels.end(), it);

  // create vectors of head/label IDs of all children
  int num_children = root->GetLength();

  // get number of children after unbinarization
  if (m_binarized) {
    num_children = 0;
    UnbinarizedChildren real_children(root, back_pointers, m_binarized);
    for (std::vector<TreePointer>::const_iterator it = real_children.begin(); it != real_children.end(); it = ++real_children) {
      num_children++;
    }
  }

  if (m_context_right && (reached_end == 1 || reached_end == 3)) num_children++; //also predict start label
  if (m_context_left && (reached_end == 2 || reached_end == 3)) num_children++; //also predict end label

  std::vector<int> heads(num_children);
  std::vector<int> labels(num_children);
  std::vector<int> heads_output(num_children);
  std::vector<int> labels_output(num_children);

  GetChildHeadsAndLabels(root, back_pointers, reached_end, lm_head, lm_label, heads, labels, heads_output, labels_output);

  //left padding; only need to add this initially
  if (reached_end == 1 || reached_end == 3) {
    std::fill_n(ngram.begin(), m_context_left, static_start_head);
    std::fill_n(ngram.begin() + m_context_left, m_context_left, static_start_label);
  }
  size_t left_padding = m_context_left;
  size_t left_offset = 0;
  size_t right_offset = std::min(heads.size(), m_context_right + 1);
  size_t right_padding = m_context_right + 1 - right_offset;

  // construct context of label model and predict label
  for (size_t i = 0; i != heads.size(); i++) {

    std::vector<int>::iterator it = ngram.begin();

    if (left_padding > 0) {
      it += left_padding;
    }

    it = std::copy(heads.begin()+left_offset, heads.begin()+i, it);

    if (left_padding > 0) {
      it += left_padding;
    }

    it = std::copy(labels.begin()+left_offset, labels.begin()+i, it);

    it = std::copy(heads.begin()+i+1, heads.begin()+right_offset, it);

    if (right_padding > 0) {
      if (reached_end == 2 || reached_end == 3) {
        std::fill_n(it, right_padding, static_stop_head);
        it += right_padding;
      } else {
        std::copy(static_label_null.begin()+offset_up_head-m_context_right-right_padding, static_label_null.begin()-m_context_right+offset_up_head, it);
      }
    }

    it = std::copy(labels.begin()+i+1, labels.begin()+right_offset, it);

    if (right_padding > 0) {
      if (reached_end == 2 || reached_end == 3) {
        std::fill_n(it, right_padding, static_stop_label);
        it += right_padding;
      } else {
        std::copy(static_label_null.begin()+offset_up_head-right_padding, static_label_null.begin()+offset_up_head, it);
      }
    }

    ngram.back() = labels_output[i];

    if (ancestor_labels.size() >= m_context_up && !num_virtual) {
      score[2] += FloorScore(lm_label->lookup_ngram(EigenMap(ngram.data(), ngram.size())));
    } else {
      boost::hash_combine(boundary_hash, ngram.back());
      score[3] += FloorScore(lm_label->lookup_ngram(EigenMap(ngram.data(), ngram.size())));
    }

    // construct context of head model and predict head
    if (heads[i] != static_start_head && heads[i] != static_stop_head && heads[i] != static_dummy_head && heads[i] != static_head_head) {

      ngram.back() = labels[i];
      ngram.push_back(heads_output[i]);

      if (ancestor_labels.size() >= m_context_up && !num_virtual) {
        score[0] += FloorScore(lm_head->lookup_ngram(EigenMap(ngram.data(), ngram.size())));
      } else {
        boost::hash_combine(boundary_hash, ngram.back());
        score[1] += FloorScore(lm_head->lookup_ngram(EigenMap(ngram.data(), ngram.size())));
      }
      ngram.pop_back();
    }

    // next time, we need to add less start symbol padding
    if (left_padding)
      left_padding--;
    else
      left_offset++;

    if (right_offset < heads.size())
      right_offset++;
    else
      right_padding++;
  }


  if (rescoring_levels == 1) {
    ancestor_heads.pop_back();
    ancestor_labels.pop_back();
    return;
  }
  // recursion
  for (std::vector<TreePointer>::const_iterator it = root->GetChildren().begin(); it != root->GetChildren().end(); ++it) {
    Score(it->get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash, num_virtual, rescoring_levels - 1);
  }
  ancestor_heads.pop_back();
  ancestor_labels.pop_back();
}

bool RDLM::GetHead(InternalTree* root, const TreePointerMap & back_pointers, std::pair<int,int> & IDs) const
{
  InternalTree *tree;

  for (std::vector<TreePointer>::const_iterator it = root->GetChildren().begin(); it != root->GetChildren().end(); ++it) {
    if ((*it)->IsLeafNT()) {
      tree = back_pointers.find(it->get())->second.get();
    } else {
      tree = it->get();
    }

    if (m_binarized && tree->GetLabel()[0] == '^') {
      bool found = GetHead(tree, back_pointers, IDs);
      if (found) {
        return true;
      }
    }

    // assumption (only true for dependency parse): each constituent has a preterminal label, and corresponding terminal is head
    // if constituent has multiple preterminals, first one is picked; if it has no preterminals, dummy_head is returned
    else if (tree->GetLength() == 1 && tree->GetChildren()[0]->IsTerminal()) {
      GetIDs(tree->GetChildren()[0]->GetLabel(), tree->GetLabel(), IDs);
      return true;
    }
  }

  return false;
}


void RDLM::GetChildHeadsAndLabels(InternalTree *root, const TreePointerMap & back_pointers, int reached_end, const nplm::neuralTM *lm_head, const nplm::neuralTM *lm_label, std::vector<int> & heads, std::vector<int> & labels, std::vector<int> & heads_output, std::vector<int> & labels_output) const
{
  std::pair<int,int> child_ids;
  size_t j = 0;

  // score start label (if enabled) for all nonterminal nodes (but not for terminal or preterminal nodes)
  if (m_context_right && (reached_end == 1 || reached_end == 3)) {
    heads[j] = static_start_head;
    labels[j] = static_start_label;
    labels_output[j] = static_start_label_output;
    j++;
  }

  UnbinarizedChildren real_children(root, back_pointers, m_binarized);

  // extract head words / labels
  for (std::vector<TreePointer>::const_iterator itx = real_children.begin(); itx != real_children.end(); itx = ++real_children) {
    if ((*itx)->IsTerminal()) {
      std::cerr << "non-terminal node " << root->GetLabel() << " has a mix of terminal and non-terminal children. This shouldn't happen..." << std::endl;
      std::cerr << "children: ";
      for (std::vector<TreePointer>::const_iterator itx2 = root->GetChildren().begin(); itx2 != root->GetChildren().end(); ++itx2) {
        std::cerr << (*itx2)->GetLabel() << " ";
      }
      std::cerr << std::endl;
      // resize vectors (should we throw exception instead?)
      heads.pop_back();
      labels.pop_back();
      heads_output.pop_back();
      labels_output.pop_back();
      continue;
    }
    InternalTree* child = itx->get();
    // also go through trees or previous hypotheses to rescore nodes for which more context has become available
    if ((*itx)->IsLeafNT()) {
      child = back_pointers.find(itx->get())->second.get();
    }

    // preterminal node
    if (child->GetLength() == 1 && child->GetChildren()[0]->IsTerminal()) {
      heads[j] = static_head_head;
      labels[j] = static_head_label;
      labels_output[j] = static_head_label_output;
      j++;
      continue;
    }

    bool found = GetHead(child, back_pointers, child_ids);
    if (!found) {
      child_ids = std::make_pair(static_dummy_head, static_dummy_head);
    }

    labels[j] = lm_head->lookup_input_word(child->GetLabel());
    labels_output[j] = lm_label->lookup_output_word(child->GetLabel());
    heads[j] = child_ids.first;
    heads_output[j] = child_ids.second;
    j++;
  }

  // score end label (if enabled) for all nonterminal nodes (but not for terminal or preterminal nodes)
  if (m_context_left && (reached_end == 2 || reached_end == 3)) {
    heads[j] = static_stop_head;
    labels[j] = static_stop_label;
    labels_output[j] = static_stop_label_output;
  }
}


void RDLM::GetIDs(const std::string & head, const std::string & preterminal, std::pair<int,int> & IDs) const
{
  IDs.first = lm_head_base_instance_->lookup_input_word(head);
  if (m_isPretermBackoff && IDs.first == 0) {
    IDs.first = lm_head_base_instance_->lookup_input_word(preterminal);
  }
  if (m_sharedVocab) {
    IDs.second = IDs.first;
  } else {
    IDs.second = lm_head_base_instance_->lookup_output_word(head);
    if (m_isPretermBackoff && IDs.second == 0) {
      IDs.second = lm_head_base_instance_->lookup_output_word(preterminal);
    }
  }
}


void RDLM::PrintInfo(std::vector<int> &ngram, nplm::neuralTM* lm) const
{
  for (size_t i = 0; i < ngram.size()-1; i++) {
    std::cerr << lm->get_input_vocabulary().words()[ngram[i]] << " ";
  }
  std::cerr << lm->get_output_vocabulary().words()[ngram.back()] << " ";

  for (size_t i = 0; i < ngram.size(); i++) {
    std::cerr << ngram[i] << " ";
  }
  std::cerr << "score: " << lm->lookup_ngram(ngram) << std::endl;
}


RDLM::TreePointerMap RDLM::AssociateLeafNTs(InternalTree* root, const std::vector<TreePointer> &previous) const
{

  TreePointerMap ret;
  std::vector<TreePointer>::iterator it;
  bool found = false;
  InternalTree::leafNT next_leafNT(root);
  for (std::vector<TreePointer>::const_iterator it_prev = previous.begin(); it_prev != previous.end(); ++it_prev) {
    found = next_leafNT(it);
    if (found) {
      ret[it->get()] = *it_prev;
    } else {
      std::cerr << "Warning: leaf nonterminal not found in rule; why did this happen?\n";
    }
  }
  return ret;
}

void RDLM::ScoreFile(std::string &path)
{
  InputFileStream inStream(path);
  std::string line, null;
  std::vector<int> ancestor_heads(m_context_up, static_root_head);
  std::vector<int> ancestor_labels(m_context_up, static_root_label);
  while(getline(inStream, line)) {
    TreePointerMap back_pointers;
    boost::array<float, 4> score;
    score.fill(0);
    InternalTree* mytree (new InternalTree(line));
    size_t boundary_hash = 0;
    Score(mytree, back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash);
    std::cerr << "head LM: " << score[0] << "label LM: " << score[2] << std::endl;
  }
}


void RDLM::SetParameter(const std::string& key, const std::string& value)
{
  std::cerr << "setting: " << this->GetScoreProducerDescription() << " - " << key << "\n";
  if (key == "tuneable") {
    m_tuneable = Scan<bool>(value);
  } else if (key == "filterable") { //ignore
  } else if (key == "path_head_lm") {
    m_path_head_lm = value;
  } else if (key == "path_label_lm") {
    m_path_label_lm = value;
  } else if (key == "backoff") {
    m_isPretermBackoff = Scan<bool>(value);
  } else if (key == "context_up") {
    m_context_up = Scan<size_t>(value);
  } else if (key == "context_left") {
    m_context_left = Scan<size_t>(value);
  } else if (key == "context_right") {
    m_context_right = Scan<size_t>(value);
  } else if (key == "debug_path") {
    m_debugPath = value;
  } else if (key == "premultiply") {
    m_premultiply = Scan<bool>(value);
  } else if (key == "rerank") {
    m_rerank = Scan<bool>(value);
  } else if (key == "normalize_head_lm") {
    m_normalizeHeadLM = Scan<bool>(value);
  } else if (key == "normalize_label_lm") {
    m_normalizeLabelLM = Scan<bool>(value);
  } else if (key == "binarized") {
    if (value == "left")
      m_binarized = 1;
    else if (value == "right")
      m_binarized = 2;
    else if (value == "full")
      m_binarized = 3;
    else
      UTIL_THROW(util::Exception, "Unknown value for argument " << key << "=" << value);
  } else if (key == "glue_symbol") {
    m_glueSymbol = value;
  } else if (key == "cache_size") {
    m_cacheSize = Scan<int>(value);
  } else {
    UTIL_THROW(util::Exception, "Unknown argument " << key << "=" << value);
  }
}


FFState* RDLM::EvaluateWhenApplied(const ChartHypothesis& cur_hypo
                                   , int featureID /* used to index the state in the previous hypotheses */
                                   , ScoreComponentCollection* accumulator) const
{
  if (const PhraseProperty *property = cur_hypo.GetCurrTargetPhrase().GetProperty("Tree")) {
    const std::string *tree = property->GetValueString();
    TreePointer mytree (boost::make_shared<InternalTree>(*tree));

    //get subtrees (in target order)
    std::vector<TreePointer> previous_trees;
    float prev_approx_head = 0, prev_approx_label = 0; //approximated (due to lack of context) LM costs from previous hypos
    for (size_t pos = 0; pos < cur_hypo.GetCurrTargetPhrase().GetSize(); ++pos) {
      const Word &word = cur_hypo.GetCurrTargetPhrase().GetWord(pos);
      if (word.IsNonTerminal()) {
        size_t nonTermInd = cur_hypo.GetCurrTargetPhrase().GetAlignNonTerm().GetNonTermIndexMap()[pos];
        const RDLMState* prev = static_cast<const RDLMState*>(cur_hypo.GetPrevHypo(nonTermInd)->GetFFState(featureID));
        previous_trees.push_back(prev->GetTree());
        prev_approx_head -= prev->GetApproximateScoreHead();
        prev_approx_label -= prev->GetApproximateScoreLabel();
      }
    }
    size_t ff_idx = m_index; // accumulator->GetIndexes(this).first;

    accumulator->PlusEquals(ff_idx, prev_approx_head);
    accumulator->PlusEquals(ff_idx+1, prev_approx_label);

    bool full_sentence = (mytree->GetChildren().back()->GetLabel() == m_endTag || (mytree->GetChildren().back()->GetLabel() == m_endSymbol && mytree->GetChildren().back()->GetChildren().back()->GetLabel() == m_endTag));
    std::vector<int> ancestor_heads ((full_sentence ? m_context_up : 0), static_root_head);
    std::vector<int> ancestor_labels ((full_sentence ? m_context_up : 0), static_root_label);
    ancestor_heads.reserve(10);
    ancestor_labels.reserve(10);

    TreePointerMap back_pointers = AssociateLeafNTs(mytree.get(), previous_trees);
    boost::array<float, 4> score; // score_head, approx_score_head, score_label, approx_score_label
    score.fill(0);
    //hash of all boundary symbols (symbols with incomplete context); trees with same hash share state for cube pruning.
    size_t boundary_hash = 0;
    if (!m_rerank) {
      Score(mytree.get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash);
      accumulator->PlusEquals(ff_idx, score[0] + score[1]);
      accumulator->PlusEquals(ff_idx+1, score[2] + score[3]);
    }
    mytree->Combine(previous_trees);
    if (m_rerank && full_sentence) {
      Score(mytree.get(), back_pointers, score, ancestor_heads, ancestor_labels, boundary_hash);
      accumulator->PlusEquals(ff_idx, score[0] + score[1]);
      accumulator->PlusEquals(ff_idx+1, score[2] + score[3]);
    }
    if (m_binarized && full_sentence) {
      mytree->Unbinarize();
    }

    return new RDLMState(mytree, score[1], score[3], boundary_hash);
  } else {
    UTIL_THROW2("Error: RDLM active, but no internal tree structure found");
  }

}

}
