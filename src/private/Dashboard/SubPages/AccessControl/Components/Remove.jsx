import Box from '@mui/material/Box'
import TextField from '@mui/material/TextField'
import Typography from '@mui/material/Typography'

export default function Remove({ value, errorMessage, onChange }) {
  return (
    <Box sx={{ display: 'flex', flexDirection: 'column', gap: 2, pt: 1 }}>
      <Typography color="text.secondary">
        Enter the hostname or URL you want to remove from the block list.
      </Typography>
      <TextField
        autoFocus
        fullWidth
        label="Site URL"
        value={value}
        onChange={(event) => onChange(event.target.value)}
        error={Boolean(errorMessage)}
        helperText={errorMessage || 'Only existing blocked sites can be removed.'}
      />
    </Box>
  )
}
