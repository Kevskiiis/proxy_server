import Box from '@mui/material/Box'
import TextField from '@mui/material/TextField'
import Typography from '@mui/material/Typography'

export default function Add({ value, errorMessage, onChange }) {
  return (
    <Box sx={{ display: 'flex', flexDirection: 'column', gap: 2, pt: 1 }}>
      <Typography color="text.secondary">
        Enter a hostname or full URL. Examples: `youtube.com` or `https://reddit.com/r/all`.
      </Typography>
      <TextField
        autoFocus
        fullWidth
        label="Site URL"
        value={value}
        onChange={(event) => onChange(event.target.value)}
        error={Boolean(errorMessage)}
        helperText={errorMessage || 'The site will be normalized to its hostname.'}
      />
    </Box>
  )
}
