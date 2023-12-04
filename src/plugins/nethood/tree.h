// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...

	N-ary tree template.
*/

#pragma once

// Protect our methods from windowsx.h macros.
#ifdef _INC_WINDOWSX
#undef GetNextSibling
#endif

/// Template for N-ary tree data structure.
template <class T>
class TTree
{
protected:
    struct _ITERATOR
    {
    };

    /// Tree node.
    class CNode : public _ITERATOR
    {
    private:
        // Private to prevent use.
        CNode(const CNode&);
        CNode& operator=(const CNode&);

    public:
        /// Pointer to the next sibling in the chain.
        CNode* m_pNextSibling;

        /// Pointer to the the first child in the chain.
        CNode* m_pFirstChild;

        /// Pointer to the parent node.
        CNode* m_pParent;

        TTree<T>* m_pTree;

        /// Data holder.
        T m_data;

        bool m_bUnlinked;

        /// Constructor.
        CNode(TTree<T>* pChief)
        {
            m_pTree = pChief;
            m_pParent = NULL;
            m_pFirstChild = NULL;
            m_pNextSibling = NULL;
            m_bUnlinked = false;
        }

        /// Constructor.
        /// \param e Node value.
        CNode(TTree<T>* pChief, const T& e) : m_data(e)
        {
            m_pTree = pChief;
            m_pParent = NULL;
            m_pFirstChild = NULL;
            m_pNextSibling = NULL;
            m_bUnlinked = false;
        }

        /// Destructor.
        ~CNode()
        {
            RemoveAllChildren();
            Unlink();
        }

        /// Unlinks this node from parent and from the list of the siblings.
        void Unlink()
        {
            CNode *pIter, *pPrev;

            if (m_bUnlinked)
                return;

            m_bUnlinked = true;

            /* unlink from the chief (root nodes only) */
            if (m_pTree->m_pRoot == this)
            {
                m_pTree->m_pRoot = m_pNextSibling;
            }

            /* unlink from sibling chain */
            if (m_pParent)
            {
                for (pIter = m_pParent->m_pFirstChild, pPrev = NULL;
                     pIter && pIter != this;
                     pPrev = pIter, pIter = pIter->m_pNextSibling)
                {
                }
                if (pPrev)
                {
                    pPrev->m_pNextSibling = m_pNextSibling;
                }
            }

            /* unlink from parent (first child only) */
            if (m_pParent && m_pParent->m_pFirstChild == this)
            {
                m_pParent->m_pFirstChild = m_pNextSibling;
            }

            /* unlink children (there should be none already) */
            for (pIter = m_pFirstChild; pIter != NULL; pIter = pIter->m_pNextSibling)
            {
                pIter->m_pParent = NULL;
            }

            m_pParent = NULL;
            m_pFirstChild = NULL;
            m_pNextSibling = NULL;
        }

        /// Links this node into the tree.
        /// \param pParent Pointer to the parent node. This may not
        ///        be NULL.
        /// \param pInsertAfter Pointer to the node after which will be
        ///        this node inserted. If this parameter is NULL, the
        ///        node will be inserted at the beginning of the child
        ///        list.
        void Insert(CNode* pParent, CNode* pInsertAfter)
        {
            assert(pParent != NULL);
            assert(pInsertAfter == NULL || pInsertAfter->m_pParent == pParent);

            if (pInsertAfter == NULL || pParent->m_pFirstChild == NULL)
            {
                m_pNextSibling = pParent->m_pFirstChild;
                pParent->m_pFirstChild = this;
            }
            else
            {
                CNode* pNode;
                for (pNode = pParent->m_pFirstChild;
                     pNode != NULL; pNode = pNode->m_pNextSibling)
                {
                    if (pNode == pInsertAfter)
                    {
                        break;
                    }
                }

                m_pNextSibling = pNode->m_pNextSibling;
                pNode->m_pNextSibling = this;
            }

            m_pParent = pParent;
        }

        /// Removes all the child nodes.
        void RemoveAllChildren()
        {
            CNode *pIter, *pNext;

            for (pIter = m_pFirstChild; pIter != NULL;)
            {
                assert(pIter->m_pParent == this);
                pNext = pIter->m_pNextSibling;
                pIter->Destroy();
                pIter = pNext;
            }
        }

        void Destroy()
        {
            RemoveAllChildren();
            Unlink();
            if (m_data.CanKill())
            {
                delete this;
            }
        }
    };

    friend CNode;

    /// Pointer to the root node. May be NULL if the root node was not
    /// inserted yet.
    CNode* m_pRoot;

public:
    /// Tree iterator.
    typedef _ITERATOR* ITERATOR;

    /// Constructor.
    TTree()
    {
        m_pRoot = NULL;
    }

    /// Destructor.
    ~TTree()
    {
        delete m_pRoot;
    }

    /// Returns the first child node.
    /// \param itPos Position of the node whose child is to be returned.
    /// \return Returns position of the child of the specified node. If
    ///         the node is the leaf node (has no children), the return
    ///         value is NULL.
    ITERATOR GetFirstChild(__in ITERATOR itPos)
    {
        return static_cast<CNode*>(itPos)->m_pFirstChild;
    }

    /// Returns the first child node.
    /// \param itPos Position of the node whose child is to be returned.
    /// \return Returns position of the child of the specified node. If
    ///         the node is the leaf node (has no children), the return
    ///         value is NULL.
    const ITERATOR GetFirstChild(__in const ITERATOR itPos) const
    {
        return static_cast<const CNode*>(itPos)->m_pFirstChild;
    }

    /// Returns the next sibling node.
    /// \param itPos Position of the node whose sibling is to be returned.
    /// \return Returns position of the next sibling node of the specified
    ///         node. If the node is the last node, the return value is
    ///         NULL.
    ITERATOR GetNextSibling(__in ITERATOR itPos)
    {
        return static_cast<CNode*>(itPos)->m_pNextSibling;
    }

    /// Returns the next sibling node.
    /// \param itPos Position of the node whose sibling is to be returned.
    /// \return Returns position of the next sibling node of the specified
    ///         node. If the node is the last node, the return value is
    ///         NULL.
    const ITERATOR GetNextSibling(__in const ITERATOR itPos) const
    {
        return static_cast<const CNode*>(itPos)->m_pNextSibling;
    }

    /// Returns the root node.
    /// \return The return value is position of the root node. If the root
    ///         node was not setup yet, the return value is NULL.
    ITERATOR GetRoot()
    {
        return m_pRoot;
    }

    /// Returns the root node.
    /// \return The return value is position of the root node. If the root
    ///         node was not setup yet, the return value is NULL.
    const ITERATOR GetRoot() const
    {
        return m_pRoot;
    }

    /// Returns the parent node.
    /// \param itPos Position of the node whose parent is to be returned.
    /// \return Returns position of the parent node of the specified
    ///         node. If the node is the root node, the return value is
    ///         NULL.
    ITERATOR GetParent(__in ITERATOR itPos)
    {
        assert(itPos);
        return static_cast<CNode*>(itPos)->m_pParent;
    }

    /// Returns the parent node.
    /// \param itPos Position of the node whose parent is to be returned.
    /// \return Returns position of the parent node of the specified
    ///         node. If the node is the root node, the return value is
    ///         NULL.
    const ITERATOR GetParent(__in const ITERATOR itPos) const
    {
        assert(itPos);
        return static_cast<const CNode*>(itPos)->m_pParent;
    }

    /// Inserts a new node in the tree.
    /// \param itParent Position of the parent node. If this parameter
    ///        is NULL the root node will be created.
    /// \param itInsertAfter Position of the node after which will be
    ///        this node inserted. If this parameter is NULL, the
    ///        node will be inserted at the beginning of the child
    ///        list.
    /// \return The return value is position of the new node.
    ITERATOR Insert(
        __in_opt ITERATOR itParent,
        __in_opt ITERATOR itInsertAfter)
    {
        CNode* pNewNode = new CNode(this);
        if (itParent == NULL)
        {
            assert(m_pRoot == NULL);
            assert(itInsertAfter == NULL);
            m_pRoot = pNewNode;
        }
        else
        {
            pNewNode->Insert(static_cast<CNode*>(itParent),
                             static_cast<CNode*>(itInsertAfter));
        }
        return pNewNode;
    }

    /// Inserts a new node in the tree.
    /// \param itParent Position of the parent node. If this parameter
    ///        is NULL the root node will be created.
    /// \param itInsertAfter Position of the node after which will be
    ///        this node inserted. If this parameter is NULL, the
    ///        node will be inserted at the beginning of the child
    ///        list.
    /// \param e Value of the new node.
    /// \return The return value is position of the new node.
    ITERATOR Insert(
        __in_opt ITERATOR itParent,
        __in_opt ITERATOR itInsertAfter,
        __in const T& e)
    {
        CNode* pNewNode = new CNode(this, e);
        if (itParent == NULL)
        {
            assert(m_pRoot == NULL);
            assert(itInsertAfter == NULL);
            m_pRoot = pNewNode;
        }
        else
        {
            pNewNode->Insert(static_cast<CNode*>(itParent),
                             static_cast<CNode*>(itInsertAfter));
        }
        return pNewNode;
    }

    /// Returns value of the node.
    /// \param itPos Position of the node whose value is to be returned.
    /// \return Returns value of the node at the specified position.
    T& GetAt(__in ITERATOR itPos)
    {
        return static_cast<CNode*>(itPos)->m_data;
    }

    /// Returns value of the node.
    /// \param itPos Position of the node whose value is to be returned.
    /// \return Returns value of the node at the specified position.
    const T& GetAt(__in const ITERATOR itPos) const
    {
        return static_cast<const CNode*>(itPos)->m_data;
    }

    /// Removes node and all of its child nodes.
    /// \param itPos Position of the node to be removed.
    void Remove(__in ITERATOR itPos)
    {
        assert(itPos != NULL);

        CNode* pNode = static_cast<CNode*>(itPos);

        pNode->Destroy();
    }
};
