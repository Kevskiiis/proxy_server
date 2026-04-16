import { useMemo, useState } from 'react'
import Alert from '@mui/material/Alert'
import Box from '@mui/material/Box'
import Button from '@mui/material/Button'
import Dialog from '@mui/material/Dialog'
import DialogActions from '@mui/material/DialogActions'
import DialogContent from '@mui/material/DialogContent'
import DialogTitle from '@mui/material/DialogTitle'
import Paper from '@mui/material/Paper'
import Snackbar from '@mui/material/Snackbar'
import Table from '@mui/material/Table'
import TableBody from '@mui/material/TableBody'
import TableCell from '@mui/material/TableCell'
import TableContainer from '@mui/material/TableContainer'
import TableHead from '@mui/material/TableHead'
import TableRow from '@mui/material/TableRow'
import Typography from '@mui/material/Typography'

// Components: ===================================
import Add from './Components/Add'
import Remove from './Components/Remove'
import { formatDate, normalizeSiteInput } from '../../../../utils/blocklist'

export default function AccessControl({ rows, onAddSite, onRemoveSite }) {
  const [dialogMode, setDialogMode] = useState(null)
  const [siteInput, setSiteInput] = useState('')
  const [errorMessage, setErrorMessage] = useState('')
  const [feedback, setFeedback] = useState(null)
  const [isSubmitting, setIsSubmitting] = useState(false)

  const knownSites = useMemo(() => new Set(rows.map((row) => row.site)), [rows])

  const openDialog = (mode, initialValue = '') => {
    setDialogMode(mode)
    setSiteInput(initialValue)
    setErrorMessage('')
  }

  const closeDialog = () => {
    setDialogMode(null)
    setSiteInput('')
    setErrorMessage('')
  }

  const handleAdd = async () => {
    try {
      const normalizedSite = normalizeSiteInput(siteInput)

      if (knownSites.has(normalizedSite)) {
        setErrorMessage('That site is already blocked.')
        return
      }

      setIsSubmitting(true)
      await onAddSite(normalizedSite)
      setFeedback({ severity: 'success', message: `${normalizedSite} added.` })
      closeDialog()
    } catch (error) {
      setErrorMessage(error.message)
    } finally {
      setIsSubmitting(false)
    }
  }

  const handleRemove = async () => {
    try {
      const normalizedSite = normalizeSiteInput(siteInput)

      if (!knownSites.has(normalizedSite)) {
        setErrorMessage('That site is not in the block list.')
        return
      }

      setIsSubmitting(true)
      await onRemoveSite(normalizedSite)
      setFeedback({ severity: 'info', message: `${normalizedSite} removed.` })
      closeDialog()
    } catch (error) {
      setErrorMessage(error.message)
    } finally {
      setIsSubmitting(false)
    }
  }

  const submitDialog = async () => {
    if (dialogMode === 'Add') {
      await handleAdd()
      return
    }

    if (dialogMode === 'Remove') {
      await handleRemove()
    }
  }

  return (
    <Box sx={{ display: 'flex', flexDirection: 'column', gap: 3 }}>
      <Box
        sx={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: { xs: 'flex-start', md: 'center' },
          flexDirection: { xs: 'column', md: 'row' },
          gap: 2,
        }}
      >
        <Box>
          <Typography variant="h4" gutterBottom>
            Access Control
          </Typography>
          <Typography color="text.secondary">
            Manage the sites currently blocked in this browser session.
          </Typography>
        </Box>
        <Box sx={{ display: 'flex', gap: 1.5 }}>
          <Button variant="contained" onClick={() => openDialog('Add')}>
            Add Site
          </Button>
          <Button variant="outlined" onClick={() => openDialog('Remove')}>
            Remove Site
          </Button>
        </Box>
      </Box>

      <Dialog open={Boolean(dialogMode)} onClose={isSubmitting ? undefined : closeDialog} maxWidth="sm" fullWidth>
        <DialogTitle>
          {dialogMode === 'Add' ? 'Add a site to the blocker' : 'Remove a site from the blocker'}
        </DialogTitle>
        <DialogContent>
          {dialogMode === 'Add' ? (
            <Add
              value={siteInput}
              errorMessage={errorMessage}
              onChange={setSiteInput}
            />
          ) : (
            <Remove
              value={siteInput}
              errorMessage={errorMessage}
              onChange={setSiteInput}
            />
          )}
        </DialogContent>
        <DialogActions>
          <Button disabled={isSubmitting} onClick={closeDialog}>Cancel</Button>
          <Button disabled={isSubmitting} variant="contained" onClick={submitDialog}>
            {dialogMode === 'Add' ? 'Add' : 'Remove'}
          </Button>
        </DialogActions>
      </Dialog>

      <TableContainer component={Paper}>
        <Table sx={{ minWidth: 650 }} aria-label="blocked sites table">
          <TableHead>
            <TableRow>
              <TableCell>ID</TableCell>
              <TableCell>Site</TableCell>
              <TableCell>Date added</TableCell>
              <TableCell align="right">Actions</TableCell>
            </TableRow>
          </TableHead>
          <TableBody>
            {rows.length === 0 ? (
              <TableRow>
                <TableCell colSpan={4}>
                  <Typography color="text.secondary">
                    No blocked sites yet. Add one to get started.
                  </Typography>
                </TableCell>
              </TableRow>
            ) : (
              rows.map((row) => (
                <TableRow
                  key={row.id}
                  sx={{ '&:last-child td, &:last-child th': { border: 0 } }}
                >
                  <TableCell component="th" scope="row">
                    {row.id}
                  </TableCell>
                  <TableCell>{row.site}</TableCell>
                  <TableCell>{formatDate(row.dateAdded)}</TableCell>
                  <TableCell align="right">
                    <Button
                      color="error"
                      onClick={() => openDialog('Remove', row.site)}
                    >
                      Remove
                    </Button>
                  </TableCell>
                </TableRow>
              ))
            )}
          </TableBody>
        </Table>
      </TableContainer>

      <Snackbar
        autoHideDuration={2500}
        open={Boolean(feedback)}
        onClose={() => setFeedback(null)}
      >
        {feedback ? (
          <Alert
            severity={feedback.severity}
            variant="filled"
            onClose={() => setFeedback(null)}
          >
            {feedback.message}
          </Alert>
        ) : null}
      </Snackbar>
    </Box>
  )
}
